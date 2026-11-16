/* SPDX-License-Identifier: MIT */
#import <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <unistd.h>
@interface NSObject(SFFXFontFamilyCatalog)
+(id)sharedFontManager;
-(id)availableFontFamilies;
-(id)availableMembersOfFontFamily:(NSString *)name;
@end
int main(void) {
    alarm(20);
    @autoreleasepool {
        id manager=[(id)objc_getClass("NSFontManager") sharedFontManager];
        if(!manager)return 2;
        NSArray *families=[manager availableFontFamilies];
        NSMutableArray *rows=[NSMutableArray array];
        for(NSString *name in [NSArray arrayWithObjects:@"Georgia",@"Times",@"Times New Roman",@"serif",@"Nimbus Roman",@"Inter",nil]){
            NSArray *members=[manager availableMembersOfFontFamily:name];
            [rows addObject:[NSDictionary dictionaryWithObjectsAndKeys:name,@"requested",
                [NSNumber numberWithBool:[families containsObject:name]],@"enumerated",
                members?:[NSArray array],@"members",nil]];
        }
        NSDictionary *result=[NSDictionary dictionaryWithObjectsAndKeys:rows,@"families",
            [NSNumber numberWithUnsignedLong:[families count]],@"physical_or_virtual_count",nil];
        NSData *data=[NSJSONSerialization dataWithJSONObject:result options:0 error:NULL];
        if(!data)return 3;
        fwrite([data bytes],1,[data length],stdout);fputc('\n',stdout);
    }
    return 0;
}
