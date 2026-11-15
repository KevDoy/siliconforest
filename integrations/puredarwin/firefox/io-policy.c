/* SPDX-License-Identifier: MIT
 * Process-local diagnostic for background-I/O stalls. Calls the real API and
 * never reports a policy change the kernel did not accept. Off unless selected.
 */
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
static int (*real_set)(int,int,int);
static pthread_once_t once=PTHREAD_ONCE_INIT;
static void resolve(void){real_set=dlsym(RTLD_NEXT,"setiopolicy_np");}
static int sffx_set(int type,int scope,int policy){
    pthread_once(&once,resolve);
    if(!real_set){errno=ENOSYS;return -1;}
    int effective=policy;
    const char *mode=getenv("SFFX_IO_POLICY");
    if(type==IOPOL_TYPE_DISK&&scope==IOPOL_SCOPE_THREAD&&policy==IOPOL_THROTTLE&&mode){
        if(!strcmp(mode,"utility"))effective=IOPOL_UTILITY;
        if(!strcmp(mode,"standard"))effective=IOPOL_STANDARD;
        if(!strcmp(mode,"default"))effective=IOPOL_DEFAULT;
    }
    int result=real_set(type,scope,effective),saved=errno;
    if(getenv("SFFX_IO_TRACE"))fprintf(stderr,"FXIOPOL type=%d scope=%d requested=%d effective=%d status=%d actual=%d\n",type,scope,policy,effective,result,getiopolicy_np(type,scope));
    errno=saved;return result;
}
__attribute__((used,section("__DATA,__interpose")))
static const struct{const void *replacement,*original;} interpose={
    (const void *)sffx_set,(const void *)setiopolicy_np
};
