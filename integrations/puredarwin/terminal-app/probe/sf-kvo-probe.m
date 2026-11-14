// SPDX-License-Identifier: MIT
// Status 0 only with a Foundation whose key-value observing follows a key path and hears a
// setter that is called directly (../patches/foundation-macos-applications.patch): the
// installer's proof that a process gets the new library (../tools/sf_terminal.sh).
#import <Foundation/Foundation.h>
#include <stdio.h>

@interface SFLeaf : NSObject { NSString *_name; }
- (NSString *)name;
- (void)setName:(NSString *)name;
@end
@implementation SFLeaf
- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { [name retain]; [_name release]; _name = name; }
@end
@interface SFRoot : NSObject { SFLeaf *_leaf; @public int heard; }
- (SFLeaf *)leaf;
- (void)setLeaf:(SFLeaf *)leaf;
@end
@implementation SFRoot
- (SFLeaf *)leaf { return _leaf; }
- (void)setLeaf:(SFLeaf *)leaf { [leaf retain]; [_leaf release]; _leaf = leaf; }
- (void)observeValueForKeyPath:(NSString *)path ofObject:(id)object change:(NSDictionary *)change context:(void *)context { heard++; }
@end

int main(void)
{
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    SFRoot *root = [[SFRoot alloc] init];
    SFLeaf *first = [[SFLeaf alloc] init], *second = [[SFLeaf alloc] init];
    [root setLeaf:first];
    [root addObserver:root forKeyPath:@"leaf.name" options:NSKeyValueObservingOptionNew context:NULL];
    [first setName:@"a"];                 // a setter called directly, one component down the path
    int afterLeaf = root->heard;
    [root setLeaf:second];                // the first component replaced
    int afterSwap = root->heard;
    [second setName:@"b"];                // the new value is observed
    [first setName:@"c"];                 // the old one no longer is
    int end = root->heard;
    [root removeObserver:root forKeyPath:@"leaf.name"];
    [second setName:@"d"];
    BOOL ok = afterLeaf == 1 && afterSwap == 2 && end == 3 && root->heard == 3;
    printf("sf-kvo-probe: notifications %d, %d, %d, %d (wanted 1, 2, 3, 3): %s\n", afterLeaf, afterSwap, end, root->heard, ok ? "ok" : "FAIL");
    [pool release];
    return ok ? 0 : 1;
}
