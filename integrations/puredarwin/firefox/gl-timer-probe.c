/* SPDX-License-Identifier: MIT
 * Native CGL/GL timestamp API control. Public ABI declarations, no Apple SDK.
 * Results establish API execution, not independent timer calibration.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define LOAD(ret,name,args) ret (*name)args=dlsym(lib,#name); \
 if(!name){fprintf(stderr,"missing %s\n",#name);return 2;}
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
int main(int argc,char **argv){
 void *lib=dlopen(argc>1?argv[1]:"/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL",RTLD_NOW|RTLD_LOCAL);
 if(!lib){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
 LOAD(int,CGLChoosePixelFormat,(const int *,void **,int *));
 LOAD(int,CGLCreateContext,(void *,void *,void **));
 LOAD(int,CGLSetCurrentContext,(void *));
 LOAD(int,CGLDestroyContext,(void *));LOAD(int,CGLDestroyPixelFormat,(void *));
 LOAD(void,glGenQueries,(int,unsigned *));LOAD(void,glDeleteQueries,(int,const unsigned *));
 LOAD(void,glQueryCounter,(unsigned,unsigned));LOAD(void,glGetQueryiv,(unsigned,unsigned,int *));
 LOAD(void,glBeginQuery,(unsigned,unsigned));LOAD(void,glEndQuery,(unsigned));
 LOAD(void,glGetQueryObjectui64v,(unsigned,unsigned,uint64_t *));
 LOAD(void,glGetQueryObjecti64v,(unsigned,unsigned,int64_t *));
 LOAD(void,glClearColor,(float,float,float,float));LOAD(void,glClear,(unsigned));
 LOAD(void,glFinish,(void));LOAD(unsigned,glGetError,(void));
 LOAD(void,glGenTextures,(int,unsigned *));LOAD(void,glBindTexture,(unsigned,unsigned));
 LOAD(void,glTexImage2D,(unsigned,int,int,int,int,int,unsigned,unsigned,const void *));
 LOAD(void,glDeleteTextures,(int,const unsigned *));
 LOAD(void,glGenFramebuffers,(int,unsigned *));LOAD(void,glBindFramebuffer,(unsigned,unsigned));
 LOAD(void,glFramebufferTexture2D,(unsigned,unsigned,unsigned,unsigned,int));
 LOAD(unsigned,glCheckFramebufferStatus,(unsigned));LOAD(void,glDeleteFramebuffers,(int,const unsigned *));
 LOAD(void,glViewport,(int,int,int,int));LOAD(void,glReadPixels,(int,int,int,int,unsigned,unsigned,void *));
 LOAD(unsigned,glCreateShader,(unsigned));LOAD(void,glShaderSource,(unsigned,int,const char **,const int *));
 LOAD(void,glCompileShader,(unsigned));LOAD(void,glGetShaderiv,(unsigned,unsigned,int *));
 LOAD(unsigned,glCreateProgram,(void));LOAD(void,glAttachShader,(unsigned,unsigned));
 LOAD(void,glLinkProgram,(unsigned));LOAD(void,glGetProgramiv,(unsigned,unsigned,int *));
 LOAD(void,glUseProgram,(unsigned));LOAD(void,glDeleteShader,(unsigned));LOAD(void,glDeleteProgram,(unsigned));
 LOAD(void,glGenVertexArrays,(int,unsigned *));LOAD(void,glBindVertexArray,(unsigned));
 LOAD(void,glDeleteVertexArrays,(int,const unsigned *));LOAD(void,glDrawArrays,(unsigned,int,int));
 LOAD(const unsigned char *,glGetString,(unsigned));
 void *format=NULL,*context=NULL;int count=0,attributes[]={99,0x4100,73,96,0};
 int status=CGLChoosePixelFormat(attributes,&format,&count);
 if(!status)status=CGLCreateContext(format,NULL,&context);
 if(!status)status=CGLSetCurrentContext(context);
 if(status||!context){fprintf(stderr,"CGL context error=%d\n",status);return 1;}
 /* A clear against an unattached default framebuffer submits no draw batch;
  * Mesa then returns its empty-query sentinel (1ns). Use real FBO storage. */
 unsigned texture=0,fbo=0;glGenTextures(1,&texture);glBindTexture(0xde1,texture);
 glTexImage2D(0xde1,0,0x8058,512,512,0,0x1908,0x1401,NULL);
 glGenFramebuffers(1,&fbo);glBindFramebuffer(0x8d40,fbo);
 glFramebufferTexture2D(0x8d40,0x8ce0,0xde1,texture,0);
 if(glCheckFramebufferStatus(0x8d40)!=0x8cd5)return 1;
 glViewport(0,0,512,512);glClearColor(0,0,0,1);glClear(0x4000);glFinish();
 const char *vs="#version 330\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}";
 const char *fs="#version 330\nout vec4 color;void main(){color=vec4(.2,.4,.6,1);}";
 unsigned vert=glCreateShader(0x8b31),frag=glCreateShader(0x8b30),program=glCreateProgram(),vao=0;int compiled=0;
 glShaderSource(vert,1,&vs,NULL);glCompileShader(vert);glGetShaderiv(vert,0x8b81,&compiled);if(!compiled)return 1;
 glShaderSource(frag,1,&fs,NULL);glCompileShader(frag);glGetShaderiv(frag,0x8b81,&compiled);if(!compiled)return 1;
 glAttachShader(program,vert);glAttachShader(program,frag);glLinkProgram(program);
 glGetProgramiv(program,0x8b82,&compiled);if(!compiled)return 1;
 glUseProgram(program);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 glDrawArrays(4,0,3);glFinish(); // Compile/warm shaders before the timed batch.
 int bits=0;glGetQueryiv(0x8e28,0x8864,&bits);
 unsigned q[3]={0};glGenQueries(3,q);uint64_t start=0,end=0,elapsed=0;int64_t signed_start=0;
 uint64_t cpu_start=now();
 glQueryCounter(q[0],0x8e28);glBeginQuery(0x88bf,q[2]);
 glClearColor(.2f,.4f,.6f,1);glClear(0x4000);glDrawArrays(4,0,3);glEndQuery(0x88bf);
 glQueryCounter(q[1],0x8e28);glFinish();
 glGetQueryObjectui64v(q[0],0x8866,&start);glGetQueryObjectui64v(q[1],0x8866,&end);
 glGetQueryObjectui64v(q[2],0x8866,&elapsed);glGetQueryObjecti64v(q[0],0x8866,&signed_start);
 unsigned char pixel[4]={0};glReadPixels(256,256,1,1,0x1908,0x1401,pixel);
 uint64_t cpu_wall=now()-cpu_start;unsigned error=glGetError();
 glQueryCounter(q[0],0x88bf);unsigned invalid=glGetError();
 int pixels= pixel[0]==51&&pixel[1]==102&&pixel[2]==153&&pixel[3]==255;
 int pass=bits>0&&start>0&&end>start&&elapsed>1&&signed_start==(int64_t)start&&error==0&&invalid==0x500&&pixels;
 printf("FXGLTIMER renderer=%s version=%s bits=%d start_ns=%llu end_ns=%llu elapsed_ns=%llu cpu_wall_ns=%llu gl_error=%x invalid_target_error=%x exact_pixel=%d pass=%d\n",
  glGetString(0x1f01),glGetString(0x1f02),bits,(unsigned long long)start,(unsigned long long)end,(unsigned long long)elapsed,(unsigned long long)cpu_wall,error,invalid,pixels,pass);
 glBindFramebuffer(0x8d40,0);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);
 glBindVertexArray(0);glDeleteVertexArrays(1,&vao);glUseProgram(0);
 glDeleteProgram(program);glDeleteShader(vert);glDeleteShader(frag);
 glDeleteQueries(3,q);CGLSetCurrentContext(NULL);CGLDestroyContext(context);CGLDestroyPixelFormat(format);
 dlclose(lib);return pass?0:1;
}
