// SPDX-License-Identifier: MIT
// Status 0 only with a Foundation whose key-value observing follows a key path and hears a
// setter that is called directly (../patches/foundation-macos-applications.patch): the
// installer's proof that a process gets the new library (../tools/sf_terminal.sh). Since 14
// November also: key-value coding reaches a scalar instance variable and the accessors named
// -_key and -_setKey:, a subclass of NSAttributedString keeps its own object, and a bundle's
// resource paths are whole paths (the second build of that patch; the first passes only the
// observing part, so the installer replaces it).
#import <Foundation/Foundation.h>
#include <stdio.h>
#import <objc/runtime.h>

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

@interface SFScalar : NSObject { int _count; BOOL _flag; int hidden; }
- (int)_hidden;
- (void)_setHidden:(int)value;
@end
@implementation SFScalar
- (int)_hidden { return hidden; }
- (void)_setHidden:(int)value { hidden = value; }
@end
@interface SFText : NSAttributedString { NSString *_mine; }
@end
@implementation SFText
- (id)init { if ((self = [super init]) != nil) _mine = @"own"; return self; }
- (NSString *)string { return _mine; }
- (NSDictionary *)attributesAtIndex:(NSUInteger)index effectiveRange:(NSRangePointer)range { if (range) *range = NSMakeRange(0, 3); return [NSDictionary dictionary]; }
@end

// the second build's additions; 0 when all hold
static int second_build(void)
{
    int bad = 0;
    SFScalar *o = [[SFScalar alloc] init];
    @try {
        [o setValue:[NSNumber numberWithInt:7] forKey:@"count"];
        [o setValue:[NSNumber numberWithBool:YES] forKey:@"flag"];
        [o setValue:[NSNumber numberWithInt:9] forKey:@"hidden"];
        if ([[o valueForKey:@"count"] intValue] != 7 || ![[o valueForKey:@"flag"] boolValue] || [[o valueForKey:@"hidden"] intValue] != 9) bad |= 1;
    } @catch (id e) { bad |= 1; }
    SFText *t = [[SFText alloc] init];
    if (object_getClass(t) != [SFText class] || [t length] != 3) bad |= 2;
    [t retain]; [t release]; [t release];
    return bad;
}

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
    int more = second_build();
    BOOL ok = afterLeaf == 1 && afterSwap == 2 && end == 3 && root->heard == 3 && more == 0;
    printf("sf-kvo-probe: notifications %d, %d, %d, %d (wanted 1, 2, 3, 3), key-value coding and subclasses %d (wanted 0): %s\n", afterLeaf, afterSwap, end, root->heard, more, ok ? "ok" : "FAIL");
    [pool release];
    return ok ? 0 : 1;
}
