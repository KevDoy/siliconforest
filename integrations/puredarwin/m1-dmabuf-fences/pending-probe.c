// SPDX-License-Identifier: MIT
// Controlled pending GPU fence: Vulkan waits on a host-signalled timeline;
// dma-buf export/import and child SCM_RIGHTS/poll must retain that dependency.
#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <vulkan/vulkan.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <spawn.h>
#include <pthread.h>
#include <stdatomic.h>
extern char **environ;
struct sync_arg {uint32_t flags;int32_t fd;};
#define EXPORT_SYNC _IOWR('b',2,struct sync_arg)
#define IMPORT_SYNC _IOW('b',3,struct sync_arg)
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line=%d expr=%s errno=%d\n",__LINE__,#x,errno);goto cleanup;} } while(0)
#define VK(x) do {VkResult r=(x);if(r!=VK_SUCCESS){fprintf(stderr,"FAIL line=%d VkResult=%d expr=%s\n",__LINE__,r,#x);goto cleanup;}}while(0)
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static double epoch;
#define TRACE(label) printf("STEP pid=%ld %s t_ms=%.3f\n",(long)getpid(),label,(now()-epoch)*1000)
static int fdmsg(int s,int *fds,int sendit){
 char c='x',control[CMSG_SPACE(2*sizeof(int))];struct iovec io={&c,1};
 struct msghdr msg={0};msg.msg_iov=&io;msg.msg_iovlen=1;msg.msg_control=control;msg.msg_controllen=sizeof(control);memset(control,0,sizeof(control));
 if(sendit){struct cmsghdr *h=CMSG_FIRSTHDR(&msg);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_RIGHTS;h->cmsg_len=CMSG_LEN(2*sizeof(int));memcpy(CMSG_DATA(h),fds,2*sizeof(int));return sendmsg(s,&msg,0)==1?0:-1;}
 if(recvmsg(s,&msg,0)!=1||msg.msg_flags&MSG_CTRUNC)return -1;struct cmsghdr *h=CMSG_FIRSTHDR(&msg);
 if(!h||h->cmsg_level!=SOL_SOCKET||h->cmsg_type!=SCM_RIGHTS||h->cmsg_len!=CMSG_LEN(2*sizeof(int)))return -1;
 memcpy(fds,CMSG_DATA(h),2*sizeof(int));return 0;
}
struct watchdog {VkDevice device;VkSemaphore semaphore;PFN_vkSignalSemaphore signal;atomic_int cancel,fired;};
static void *watchdog_run(void *arg){
 struct watchdog *w=arg;
 for(int i=0;i<100&&!atomic_load(&w->cancel);++i){struct timespec d={0,50000000};nanosleep(&d,NULL);}
 if(!atomic_load(&w->cancel)){atomic_store(&w->fired,1);VkSemaphoreSignalInfo si={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,.semaphore=w->semaphore,.value=1};fprintf(stderr,"WATCHDOG released pending timeline: %d\n",w->signal(w->device,&si));}
 return NULL;
}
static int consumer(int s){
 int rc=1,fds[2]={-1,-1};void *map=MAP_FAILED;struct pollfd p[2];
 TRACE("child_receive_begin");CHECK(fdmsg(s,fds,0)==0);TRACE("child_receive_end");p[0]=(struct pollfd){fds[0],POLLIN,0};p[1]=(struct pollfd){fds[1],POLLIN,0};
 TRACE("child_poll_zero_begin");int readiness=poll(p,2,0);TRACE("child_poll_zero_end");CHECK(readiness==0);CHECK(write(s,"P",1)==1);double start=now();int n=poll(p,2,2000);
 CHECK(n>0&&!(p[0].revents&(POLLNVAL|POLLERR|POLLHUP))&&!(p[1].revents&(POLLNVAL|POLLERR|POLLHUP)));
 // One readiness notification may arrive first; each fd must independently finish.
 for(int i=0;i<2;++i){p[i].revents=0;CHECK(poll(&p[i],1,1000)==1&&(p[i].revents&POLLIN));}
 map=mmap(NULL,4096,PROT_READ,MAP_SHARED,fds[0],0);CHECK(map!=MAP_FAILED);
 for(unsigned i=0;i<4096/4;++i)CHECK(((volatile uint32_t*)map)[i]==0x51a7c0de);
 printf("PENDING_CHILD PASS wait_ms=%.3f bytes=4096 producer_pattern=51a7c0de\n",(now()-start)*1000);rc=0;
cleanup: if(map!=MAP_FAILED)munmap(map,4096);for(int i=0;i<2;++i)if(fds[i]>=0)close(fds[i]);close(s);return rc;
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IOLBF,0);epoch=now();if(argc==3&&!strcmp(argv[1],"--consumer"))return consumer(atoi(argv[2]));
 int rc=1,db=-1,sf=-1,pair[2]={-1,-1},child_status=0;pid_t child=-1;int submitted=0,signalled=0,watching=0;pthread_t watchdog_thread;struct watchdog watchdog={0};
 VkInstance instance=VK_NULL_HANDLE;VkDevice dev=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory mem=VK_NULL_HANDLE;VkCommandPool pool=VK_NULL_HANDLE;VkSemaphore timeline=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
 PFN_vkSignalSemaphore signal_timeline=NULL;
 VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="sf-pending-fence",.apiVersion=VK_API_VERSION_1_2};
 VkInstanceCreateInfo ici={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app};VK(vkCreateInstance(&ici,NULL,&instance));
 uint32_t count=0;VK(vkEnumeratePhysicalDevices(instance,&count,NULL));CHECK(count>0&&count<16);VkPhysicalDevice physical[16];VK(vkEnumeratePhysicalDevices(instance,&count,physical));VkPhysicalDevice gpu=physical[0];
 VkPhysicalDeviceProperties prop;vkGetPhysicalDeviceProperties(gpu,&prop);printf("DEVICE %s\n",prop.deviceName);
 if(!strstr(prop.deviceName,"Apple M1")){puts("REFUSE not Apple M1");rc=77;goto cleanup;}
 const char *exts[]={"VK_KHR_external_memory_fd","VK_EXT_external_memory_dma_buf","VK_KHR_external_fence_fd"};
 uint32_t ne=0;VK(vkEnumerateDeviceExtensionProperties(gpu,NULL,&ne,NULL));VkExtensionProperties *ep=calloc(ne,sizeof(*ep));CHECK(ep);VkResult er=vkEnumerateDeviceExtensionProperties(gpu,NULL,&ne,ep);int supported=er==VK_SUCCESS;
 for(unsigned i=0;i<3;++i){int found=0;for(uint32_t j=0;j<ne;++j)found|=!strcmp(exts[i],ep[j].extensionName);if(!found){printf("MISSING %s\n",exts[i]);supported=0;}}free(ep);if(!supported){rc=77;goto cleanup;}
 uint32_t nq=0;vkGetPhysicalDeviceQueueFamilyProperties(gpu,&nq,NULL);CHECK(nq>0&&nq<32);VkQueueFamilyProperties qp[32];vkGetPhysicalDeviceQueueFamilyProperties(gpu,&nq,qp);uint32_t family=0;while(family<nq&&!(qp[family].queueFlags&VK_QUEUE_TRANSFER_BIT))family++;CHECK(family<nq);
 float priority=1;VkDeviceQueueCreateInfo qci={.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,.queueFamilyIndex=family,.queueCount=1,.pQueuePriorities=&priority};
 VkPhysicalDeviceTimelineSemaphoreFeatures tf={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,.timelineSemaphore=VK_TRUE};
 VkDeviceCreateInfo dci={.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,.pNext=&tf,.queueCreateInfoCount=1,.pQueueCreateInfos=&qci,.enabledExtensionCount=3,.ppEnabledExtensionNames=exts};VK(vkCreateDevice(gpu,&dci,NULL,&dev));
 signal_timeline=(PFN_vkSignalSemaphore)vkGetDeviceProcAddr(dev,"vkSignalSemaphore");CHECK(signal_timeline);
 PFN_vkGetMemoryFdKHR memory_fd=(PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(dev,"vkGetMemoryFdKHR");PFN_vkGetFenceFdKHR fence_fd=(PFN_vkGetFenceFdKHR)vkGetDeviceProcAddr(dev,"vkGetFenceFdKHR");CHECK(memory_fd&&fence_fd);
 VkExternalMemoryBufferCreateInfo eb={.sType=VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
 VkBufferCreateInfo bci={.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,.pNext=&eb,.size=4096,.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE};VK(vkCreateBuffer(dev,&bci,NULL,&buffer));
 VkMemoryRequirements req;vkGetBufferMemoryRequirements(dev,buffer,&req);VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(gpu,&mp);uint32_t mt=0;VkMemoryPropertyFlags want=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
 while(mt<mp.memoryTypeCount&&(!(req.memoryTypeBits&(1u<<mt))||(mp.memoryTypes[mt].propertyFlags&want)!=want))mt++;CHECK(mt<mp.memoryTypeCount);
 VkExportMemoryAllocateInfo em={.sType=VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};VkMemoryAllocateInfo mai={.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,.pNext=&em,.allocationSize=req.size,.memoryTypeIndex=mt};VK(vkAllocateMemory(dev,&mai,NULL,&mem));VK(vkBindBufferMemory(dev,buffer,mem,0));
 VkMemoryGetFdInfoKHR mfi={.sType=VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,.memory=mem,.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};VK(memory_fd(dev,&mfi,&db));CHECK(db>=0);
 VkCommandPoolCreateInfo pci={.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,.queueFamilyIndex=family};VK(vkCreateCommandPool(dev,&pci,NULL,&pool));VkCommandBuffer cmd;VkCommandBufferAllocateInfo cai={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,.commandPool=pool,.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,.commandBufferCount=1};VK(vkAllocateCommandBuffers(dev,&cai,&cmd));
 VkCommandBufferBeginInfo begin={.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};VK(vkBeginCommandBuffer(cmd,&begin));vkCmdFillBuffer(cmd,buffer,0,4096,0x51a7c0de);VkBufferMemoryBarrier barrier={.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,.dstAccessMask=VK_ACCESS_HOST_READ_BIT,.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.buffer=buffer,.offset=0,.size=4096};vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,NULL,1,&barrier,0,NULL);VK(vkEndCommandBuffer(cmd));
 VkSemaphoreTypeCreateInfo sti={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,.semaphoreType=VK_SEMAPHORE_TYPE_TIMELINE,.initialValue=0};VkSemaphoreCreateInfo sci={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,.pNext=&sti};VK(vkCreateSemaphore(dev,&sci,NULL,&timeline));
 VkExportFenceCreateInfo ef={.sType=VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO,.handleTypes=VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT};VkFenceCreateInfo fci={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,.pNext=&ef};VK(vkCreateFence(dev,&fci,NULL,&fence));
 uint64_t one=1;VkTimelineSemaphoreSubmitInfo tsi={.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,.waitSemaphoreValueCount=1,.pWaitSemaphoreValues=&one};VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
 VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.pNext=&tsi,.waitSemaphoreCount=1,.pWaitSemaphores=&timeline,.pWaitDstStageMask=&stage,.commandBufferCount=1,.pCommandBuffers=&cmd};VkQueue queue;vkGetDeviceQueue(dev,family,0,&queue);watchdog.device=dev;watchdog.semaphore=timeline;watchdog.signal=signal_timeline;CHECK(pthread_create(&watchdog_thread,NULL,watchdog_run,&watchdog)==0);watching=1;TRACE("submit_begin");VK(vkQueueSubmit(queue,1,&submit,fence));submitted=1;TRACE("submit_end");
 VkFenceGetFdInfoKHR ffi={.sType=VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR,.fence=fence,.handleType=VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT};TRACE("fence_export_begin");VK(fence_fd(dev,&ffi,&sf));TRACE("fence_export_end");CHECK(sf>=0);
 struct sync_arg im={2,sf};TRACE("reservation_import_begin");CHECK(ioctl(db,IMPORT_SYNC,&im)==0);TRACE("reservation_import_end");struct sync_arg ex={1,-1};TRACE("reservation_export_begin");CHECK(ioctl(db,EXPORT_SYNC,&ex)==0&&ex.fd>=0);TRACE("reservation_export_end");close(sf);sf=ex.fd;
 CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);posix_spawn_file_actions_t actions;CHECK(posix_spawn_file_actions_init(&actions)==0);CHECK(posix_spawn_file_actions_adddup2(&actions,pair[1],3)==0);if(pair[0]!=3)CHECK(posix_spawn_file_actions_addclose(&actions,pair[0])==0);
 char *args[]={argv[0],"--consumer","3",NULL};TRACE("spawn_begin");int sr=posix_spawn(&child,argv[0],&actions,NULL,args,environ);posix_spawn_file_actions_destroy(&actions);CHECK(sr==0);TRACE("spawn_end");close(pair[1]);pair[1]=-1;
 int fds[2]={db,sf};CHECK(fdmsg(pair[0],fds,1)==0);struct pollfd ack={pair[0],POLLIN,0};CHECK(poll(&ack,1,2000)==1);char byte;CHECK(read(pair[0],&byte,1)==1&&byte=='P');puts("PENDING_PARENT nonready confirmed before host timeline signal");struct timespec delay={0,50000000};nanosleep(&delay,NULL);
 VkSemaphoreSignalInfo signal={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,.semaphore=timeline,.value=1};VK(signal_timeline(dev,&signal));signalled=1;
 CHECK(waitpid(child,&child_status,0)==child);child=-1;CHECK(WIFEXITED(child_status)&&WEXITSTATUS(child_status)==0);CHECK(!atomic_load(&watchdog.fired));rc=0;puts("PENDING_PARENT PASS real GPU write -> reservation snapshot -> SCM_RIGHTS -> child poll/read");
cleanup:
 // Always release the queued wait before any teardown, including failed probes.
 if(submitted&&!signalled&&signal_timeline){VkSemaphoreSignalInfo signal={.sType=VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,.semaphore=timeline,.value=1};VkResult sr=signal_timeline(dev,&signal);fprintf(stderr,"cleanup timeline signal=%d\n",sr);}
 if(watching){atomic_store(&watchdog.cancel,1);pthread_join(watchdog_thread,NULL);}
 if(dev&&submitted)vkDeviceWaitIdle(dev);
 if(pair[0]>=0){close(pair[0]);pair[0]=-1;}
 if(child>0){int status;waitpid(child,&status,0);}
 if(db>=0)close(db);if(sf>=0)close(sf);for(int i=0;i<2;++i)if(pair[i]>=0)close(pair[i]);
 if(dev){if(fence)vkDestroyFence(dev,fence,NULL);if(timeline)vkDestroySemaphore(dev,timeline,NULL);if(pool)vkDestroyCommandPool(dev,pool,NULL);if(buffer)vkDestroyBuffer(dev,buffer,NULL);if(mem)vkFreeMemory(dev,mem,NULL);vkDestroyDevice(dev,NULL);}if(instance)vkDestroyInstance(instance,NULL);return rc;
}
