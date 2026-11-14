/* SPDX-License-Identifier: MIT */
/* A public signature absent from PureDarwin's NSData header. Runtime behaviour
 * remains tested, not substituted by a local implementation. */
#import <Foundation/Foundation.h>
@interface NSData (SFFinchTestDeclarations)
- (NSRange)rangeOfData:(NSData *)data options:(NSUInteger)options range:(NSRange)range;
@end
