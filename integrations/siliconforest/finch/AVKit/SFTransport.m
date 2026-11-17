/* SPDX-License-Identifier: MIT */
/* Our transport shapes and CPU fallback; they consume actual AVPlayerLayer
 * contents. No system image catalogue or fictitious playback service. */
#import <AVKit/AVKit.h>
#import "SFTransport.h"
#include <math.h>
@interface AVPlayerView(SFTransportActions)
- (void)_togglePlayback:(id)sender;
@end
@implementation SFPlayerDrawToken
@synthesize owner=_owner;
@end
@implementation SFTransportView
- (void)setPlayerView:(AVPlayerView *)view {_playerView=view;}
- (NSUInteger)drawCount{return _draws;}
- (void)drawRect:(NSRect)dirty {
 (void)dirty;_draws++;
 [[NSColor colorWithCalibratedWhite:.12 alpha:1] set];NSRectFill(self.bounds);
 [[NSColor whiteColor] set];
 if(_playerView.player.rate==0){NSBezierPath *play=[NSBezierPath bezierPath];[play moveToPoint:NSMakePoint(15,9)];[play lineToPoint:NSMakePoint(15,29)];[play lineToPoint:NSMakePoint(32,19)];[play closePath];[play fill];}
 else {NSRectFill(NSMakeRect(15,9,5,20));NSRectFill(NSMakeRect(25,9,5,20));}
 CGFloat width=NSWidth(self.bounds)-70;if(width<1)return;
 [[NSColor colorWithCalibratedWhite:.35 alpha:1] set];NSRectFill(NSMakeRect(50,16,width,6));
 double duration=CMTimeGetSeconds(_playerView.player.currentItem.duration),time=CMTimeGetSeconds(_playerView.player.currentTime);
 double fraction=isfinite(duration)&&duration>0&&isfinite(time)?MAX(0,MIN(1,time/duration)):0;
 [[NSColor colorWithCalibratedRed:.2 green:.65 blue:1 alpha:1] set];NSRectFill(NSMakeRect(50,16,width*fraction,6));
}
- (void)mouseDown:(NSEvent *)event {
 NSPoint point=[self convertPoint:[event locationInWindow] fromView:nil];
 if(point.x<42){[_playerView _togglePlayback:self];return;}
 CGFloat width=NSWidth(self.bounds)-70;double duration=CMTimeGetSeconds(_playerView.player.currentItem.duration);
 if(width>0&&isfinite(duration)&&duration>0&&point.x>=50&&point.x<=50+width){double fraction=(point.x-50)/width;[_playerView.player seekToTime:CMTimeMakeWithSeconds(fraction*duration,600)];[self setNeedsDisplay:YES];}
}
@end
BOOL SFDrawPlayerContents(AVPlayerView *view,AVPlayerLayer *layer){
 CGImageRef image=(CGImageRef)[layer contents];if(!image)return NO;
 CGContextRef context=[[NSGraphicsContext currentContext] graphicsPort];if(!context)return NO;
 CGRect box=NSRectToCGRect(view.bounds);CGFloat controls=view.controlsStyle==AVPlayerViewControlsStyleNone?0:38;
 box.origin.y+=controls;box.size.height=MAX(0,box.size.height-controls);
 CGFloat width=CGImageGetWidth(image),height=CGImageGetHeight(image);if(width<=0||height<=0||box.size.height<=0)return NO;
 if(![view.videoGravity isEqualToString:AVLayerVideoGravityResize]){
  CGFloat scale=[view.videoGravity isEqualToString:AVLayerVideoGravityResizeAspectFill]?MAX(box.size.width/width,box.size.height/height):MIN(box.size.width/width,box.size.height/height);
  CGFloat w=width*scale,h=height*scale;box=CGRectMake(CGRectGetMidX(box)-w/2,CGRectGetMidY(box)-h/2,w,h);
 }
 CGContextSaveGState(context);CGContextClipToRect(context,CGRectMake(NSMinX(view.bounds),NSMinY(view.bounds)+controls,NSWidth(view.bounds),MAX(0,NSHeight(view.bounds)-controls)));
 CGContextDrawImage(context,box,image);CGContextRestoreGState(context);return YES;
}
