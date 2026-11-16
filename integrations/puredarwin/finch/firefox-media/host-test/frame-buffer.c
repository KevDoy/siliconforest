/* SPDX-License-Identifier: MIT
 * Execute the literal patched diagnostic logger with fake GL/clock/write.
 * This tests bounded buffering and I/O timing policy, not GPU execution.
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef int GLint;typedef unsigned GLuint;typedef unsigned GLenum;
typedef struct Context {int width,height;} *CGLContextObj;
typedef void *IOSurfaceRef;
enum {GL_READ_FRAMEBUFFER_BINDING=1,GL_PIXEL_PACK_BUFFER_BINDING,GL_PACK_ALIGNMENT,
 GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_READ_FRAMEBUFFER,
 GL_READ_BUFFER,GL_COLOR_ATTACHMENT0,GL_PIXEL_PACK_BUFFER,GL_RGBA,GL_UNSIGNED_BYTE};
static unsigned source_frame,writes;static int short_write;static void (*finalizer)(void);
static char captured[3][1200*1024];static size_t lengths[3];
static double pd_cgl_now(void){static double t;t+=.01;return t;}
static uint32_t surface_id(IOSurfaceRef p){(void)p;return 3;}
static struct {uint32_t (*surfaceID)(IOSurfaceRef);} pd_iosurface={surface_id};
static void glGetIntegerv(unsigned p,int *v){(void)p;*v=0;}
static void glBindFramebuffer(unsigned p,unsigned v){(void)p;(void)v;}
static void glReadBuffer(unsigned p){(void)p;}
static void glBindBuffer(unsigned p,unsigned v){(void)p;(void)v;}
static void glPixelStorei(unsigned p,int v){(void)p;(void)v;}
static unsigned glGetError(void){return 0;}
static void glReadPixels(int x,int y,int pixels,int h,unsigned format,unsigned type,void *out){
 (void)x;(void)h;(void)format;(void)type;unsigned gray=source_frame^(source_frame>>1);
 unsigned bits[18]={1,1,0,1};for(unsigned i=0;i<10;i++)bits[4+i]=(gray>>i)&1;
 bits[14]=0;bits[15]=0;bits[16]=1;bits[17]=0;
 unsigned char *p=out;for(int i=0;i<pixels;i++){
  unsigned bit=bits[(unsigned)i*18/(unsigned)pixels]^(y>350);
  p[4*i]=p[4*i+1]=p[4*i+2]=bit?224:32;p[4*i+3]=255;
 }
}
static int fake_atexit(void (*fn)(void)){assert(!finalizer);finalizer=fn;return 0;}
static ssize_t fake_write(int fd,const void *p,size_t n){
 assert(fd==STDERR_FILENO&&writes<3&&n<sizeof(captured[0]));
 size_t stored=(short_write&&writes==1)?n-1:n;
 memcpy(captured[writes],p,stored);lengths[writes]=stored;writes++;
 return (ssize_t)stored;
}
#define atexit fake_atexit
#define write fake_write
#include "private-buffered-frame-export.h"
#undef write
#undef atexit
int main(int argc,char **argv){
 struct Context ctx={1920,1440};
 if(argc==2&&!strcmp(argv[1],"disabled")){
  unsetenv("SFFX_FRAME_ID_GEOMETRY");unsetenv("SFFX_FRAME_ID_DIMENSIONS");
  sffx_frame_export(&ctx,NULL,1,1);assert(writes==0&&!finalizer);
  puts("BUFFER_HOST disabled: no write or finalizer");return 0;
 }
 assert(!setenv("SFFX_FRAME_ID_GEOMETRY","1920,1440,24,208,1440,810,1",1));
 unsetenv("SFFX_FRAME_ID_DIMENSIONS");
 short_write=argc==2&&!strcmp(argv[1],"short");
 unsigned count=argc==2&&!strcmp(argv[1],"cap")?1300:900;
 for(unsigned i=0;i<count;i++){
  source_frame=i%900;sffx_frame_export(&ctx,NULL,1,1);
  assert(writes==1); // Only the first frame0 admission writes before finalizer.
 }
 assert(finalizer&&strstr(captured[0],"FXFRAMEID_PREVIEW "));
 finalizer();assert(writes==3);
 assert(strstr(captured[2],count==900?"records=900 ":"records=1200 "));
 assert(strstr(captured[2],"overflow=0 "));
 char expected[64];snprintf(expected,sizeof(expected),"write_return=%zu ",lengths[1]);
 assert(strstr(captured[2],expected));
 if(count==900&&!short_write){
  assert(strstr(captured[1],"frame=0 gl_error=0 reason=ok"));
  assert(strstr(captured[1],"frame=899 gl_error=0 reason=ok"));
  fwrite(captured[1],1,lengths[1],stdout);fwrite(captured[2],1,lengths[2],stdout);
 }else printf("BUFFER_HOST %s: bounded/deferred writes and exact return retained\n",argv[1]);
 return 0;
}
