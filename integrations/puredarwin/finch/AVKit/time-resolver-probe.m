/* SPDX-License-Identifier: MIT */
/* Our scalar-only probe, using types observed in read-only runtime metadata.
 * No player/controller, device, window or service is created. */
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <math.h>
@interface NSObject(SFTimeResolverProbe)
- (double)currentTime;
- (double)currentTimeWithinEndTimes;
- (double)remainingTime;
- (double)remainingTimeWithinEndTimes;
- (double)remainingTargetTimeWithinEndTimes;
- (double)targetTime;
- (double)targetTimeWithinEndTimes;
- (double)minTime;
- (double)maxTime;
- (double)interval;
- (double)resolution;
- (double)seekableTimeRangeDuration;
- (BOOL)isCurrentTimeAtEndOfSeekableTimeRanges;
- (id)playerController;
- (void)setCurrentTime:(double)value;
- (void)setCurrentTimeWithinEndTimes:(double)value;
- (void)setTargetTime:(double)value;
- (void)setTargetTimeWithinEndTimes:(double)value;
- (void)setMinTime:(double)value;
- (void)setMaxTime:(double)value;
- (void)setInterval:(double)value;
- (void)setResolution:(double)value;
@end
static void printState(id r,const char *name,unsigned index) {
 printf("TIME case=%s index=%u min=%.17g max=%.17g interval=%.17g resolution=%.17g current=%.17g current-within=%.17g target=%.17g target-within=%.17g remaining=%.17g remaining-within=%.17g target-remaining=%.17g seekable-duration=%.17g at-end=%d controller-nil=%d\n",
  name,index,[r minTime],[r maxTime],[r interval],[r resolution],
  [r currentTime],[r currentTimeWithinEndTimes],[r targetTime],
  [r targetTimeWithinEndTimes],[r remainingTime],[r remainingTimeWithinEndTimes],
  [r remainingTargetTimeWithinEndTimes],[r seekableTimeRangeDuration],
  [r isCurrentTimeAtEndOfSeekableTimeRanges],[r playerController]==nil);
}
int main(void) {
 @autoreleasepool {
  void *kit=dlopen("/System/Library/Frameworks/AVKit.framework/Versions/A/AVKit",RTLD_NOW|RTLD_GLOBAL);
  Class cls=NSClassFromString(@"AVPlayerControllerTimeResolver");
  if(!kit||!cls)return 77;
  id r=[[cls alloc] init];if(!r)return 1;
  printState(r,"initial",0);
  [r setMinTime:10];[r setMaxTime:30];
  printState(r,"bounds",0);
  const double values[]={-12,0,9.9,10,10.25,17.125,29.9,30,50};
  for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){
   [r setCurrentTime:values[i]];printState(r,"current",i);
   [r setTargetTime:values[i]];printState(r,"target",i);
   [r setCurrentTimeWithinEndTimes:values[i]];printState(r,"current-within",i);
   [r setTargetTimeWithinEndTimes:values[i]];printState(r,"target-within",i);
  }
  [r setInterval:0.5];[r setResolution:0.25];printState(r,"precision",0);
  [r setMinTime:30];[r setMaxTime:10];printState(r,"inverted",0);
  [r release];return 0;
 }
}
