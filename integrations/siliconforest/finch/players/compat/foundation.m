/* SPDX-License-Identifier: MIT */
#import <Foundation/Foundation.h>
NSString *const NSAppleScriptErrorNumber=@"NSAppleScriptErrorNumber";
NSString *const NSAppleScriptErrorMessage=@"NSAppleScriptErrorMessage";
@interface NSAppleScript:NSObject {NSString *_source;} @end
@implementation NSAppleScript
- (id)initWithSource:(NSString*)source{if((self=[super init]))_source=[source copy];return self;}
- (void)dealloc{[_source release];[super dealloc];}
- (NSString*)source{return _source;}
- (BOOL)isCompiled{return NO;}
- (void)sfError:(NSDictionary**)error{if(error)*error=@{NSAppleScriptErrorNumber:@(-1753),NSAppleScriptErrorMessage:@"AppleScript runtime is unavailable on SiliconForest"};}
- (BOOL)compileAndReturnError:(NSDictionary**)error{[self sfError:error];return NO;}
- (id)executeAndReturnError:(NSDictionary**)error{[self sfError:error];return nil;}
- (id)executeAppleEvent:(id)event error:(NSDictionary**)error{[self sfError:error];return nil;}
@end
