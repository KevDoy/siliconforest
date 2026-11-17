/* SPDX-License-Identifier: MIT */
#include "sfave/sfave_ioctl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
int main(void) {
 int fd=open("/dev/sfave",O_RDONLY);
 if(fd<0){fprintf(stderr,"SFAVE_INFO open: %s; hardware encode unqualified\n",strerror(errno));return errno==ENOENT?77:1;}
 struct sfave_info info={0};int result=ioctl(fd,SFAVE_IOC_INFO,&info);close(fd);
 if(result<0){perror("SFAVE_INFO ioctl");return 1;}
 if(info.abi!=SFAVE_ABI_VERSION||info.size!=sizeof info||info.reserved){fputs("SFAVE_INFO ABI mismatch\n",stderr);return 1;}
 info.firmware[47]=info.rtkit[47]=info.iboot[47]=0;
 printf("SFAVE_INFO abi=%u bytes=%u flags=0x%x caps=0x%x soc=0x%x segments=%u error=%u\n",info.abi,info.size,info.flags,info.caps,info.soc,info.segments,info.error);
 printf("PRELOAD TEXT=0x%x flags=%u DATA=0x%x flags=%u\n",info.segment_size[0],info.segment_flags[0],info.segment_size[1],info.segment_flags[1]);
 printf("IDENTITY iboot=%s firmware=%s rtkit=%s\n",info.iboot,info.firmware,info.rtkit);
 printf("QUALIFICATION diagnostic-only; encoder-ready=%u cold-reserved=%u\n",!!(info.flags&SFAVE_INFO_ENCODER_READY),!!(info.flags&SFAVE_INFO_COLD_RESERVED));
 return info.error?1:0;
}
