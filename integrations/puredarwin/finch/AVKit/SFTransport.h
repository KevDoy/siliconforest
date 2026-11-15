/* SPDX-License-Identifier: MIT */
#import <AppKit/AppKit.h>
@class AVPlayerView,AVPlayerLayer;
@interface SFTransportView:NSView { AVPlayerView *_playerView;NSUInteger _draws; }
- (void)setPlayerView:(AVPlayerView *)view;
- (NSUInteger)drawCount;
@end
@interface SFPlayerDrawToken:NSObject { AVPlayerView *_owner; }
@property(assign) AVPlayerView *owner;
@end
BOOL SFDrawPlayerContents(AVPlayerView *view,AVPlayerLayer *layer);
