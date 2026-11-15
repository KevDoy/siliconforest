/* SPDX-License-Identifier: MIT */
#import <Foundation/Foundation.h>
#include "lease.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
extern void IOSurfaceIncrementUseCount(IOSurfaceRef);
extern void IOSurfaceDecrementUseCount(IOSurfaceRef);
extern Boolean IOSurfaceIsInUse(IOSurfaceRef);
struct SFFrameLease {AVFrame *frame;IOSurfaceRef surface;struct SFFrameLease *next;};
static pthread_mutex_t leaseLock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t leaseWake=PTHREAD_COND_INITIALIZER;
static pthread_once_t leaseOnce=PTHREAD_ONCE_INIT;
static struct SFFrameLease *pending;
static int workerReady;
static uint64_t issued,released,deferred;
static void drop(struct SFFrameLease *lease) {
 av_frame_free(&lease->frame);CFRelease(lease->surface);free(lease);
 __atomic_fetch_add(&released,1,__ATOMIC_RELEASE);
}
static void *worker(void *unused) {
 (void)unused;
 for(;;){
  pthread_mutex_lock(&leaseLock);
  while(!pending)pthread_cond_wait(&leaseWake,&leaseLock);
  pthread_mutex_unlock(&leaseLock);
  usleep(5000);
  struct SFFrameLease *ready=NULL;
  pthread_mutex_lock(&leaseLock);
  struct SFFrameLease **cursor=&pending;
  while(*cursor){
   struct SFFrameLease *lease=*cursor;
   /* GetUseCount is process-local. Only IsInUse observes other processes.
    * The consumer must retain its use until GPU completion and establish its
    * hold before the producer drops its own; this predicate is not a fence. */
   if(!IOSurfaceIsInUse(lease->surface)){*cursor=lease->next;lease->next=ready;ready=lease;}
   else cursor=&lease->next;
  }
  pthread_mutex_unlock(&leaseLock);
  while(ready){struct SFFrameLease *next=ready->next;drop(ready);ready=next;}
 }
 return NULL;
}
static void startWorker(void){pthread_t thread;if(!pthread_create(&thread,NULL,worker,NULL)){pthread_detach(thread);workerReady=1;}}
@interface SFVTCaptureFrameLease:NSObject {struct SFFrameLease *_lease;}
- (id)initWithLease:(struct SFFrameLease *)lease;
@end
@implementation SFVTCaptureFrameLease
- (id)initWithLease:(struct SFFrameLease *)lease {if((self=[super init]))_lease=lease;return self;}
- (void)dealloc {
 struct SFFrameLease *lease=_lease;
 if(lease){
  IOSurfaceDecrementUseCount(lease->surface);
  if(!IOSurfaceIsInUse(lease->surface))drop(lease);
  else {
   __atomic_fetch_add(&deferred,1,__ATOMIC_RELAXED);
   pthread_mutex_lock(&leaseLock);lease->next=pending;pending=lease;pthread_cond_signal(&leaseWake);pthread_mutex_unlock(&leaseLock);
  }
 }
 [super dealloc];
}
@end
CFTypeRef SFVTCreateFrameLease(const AVFrame *frame,IOSurfaceRef surface){
 if(!frame||!frame->buf[0]||!surface)return NULL;
 pthread_once(&leaseOnce,startWorker);if(!workerReady)return NULL;
 struct SFFrameLease *lease=calloc(1,sizeof(*lease));if(!lease)return NULL;
 lease->frame=av_frame_clone(frame);if(!lease->frame){free(lease);return NULL;}
 lease->surface=(IOSurfaceRef)CFRetain(surface);IOSurfaceIncrementUseCount(surface);
 SFVTCaptureFrameLease *object=[[SFVTCaptureFrameLease alloc] initWithLease:lease];
 if(!object){IOSurfaceDecrementUseCount(surface);av_frame_free(&lease->frame);CFRelease(surface);free(lease);return NULL;}
 __atomic_fetch_add(&issued,1,__ATOMIC_RELAXED);return (CFTypeRef)object;
}
void SFVTFrameLeaseCounts(uint64_t *a,uint64_t *b,uint64_t *c){
 if(a)*a=__atomic_load_n(&issued,__ATOMIC_ACQUIRE);
 if(b)*b=__atomic_load_n(&released,__ATOMIC_ACQUIRE);
 if(c)*c=__atomic_load_n(&deferred,__ATOMIC_ACQUIRE);
}
