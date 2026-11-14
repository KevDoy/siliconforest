/* SPDX-License-Identifier: MIT */
#import <CoreGraphics/CoreGraphics.h>
#import <dlfcn.h>
static inline CFStringRef SFCICopyColorSpaceName(CGColorSpaceRef space) {
    typedef CFStringRef (*CopyName)(CGColorSpaceRef);
    CopyName copy=(CopyName)dlsym(RTLD_DEFAULT,"CGColorSpaceCopyName");
    if (copy) return copy(space);
    CFStringRef name=CGColorSpaceGetName(space);
    return name ? CFRetain(name) : NULL;
}
static inline CGColorRef SFCICopyColorMatched(CGColorSpaceRef space,CGColorRenderingIntent intent,CGColorRef color,CFDictionaryRef options) {
    typedef CGColorRef (*Match)(CGColorSpaceRef,CGColorRenderingIntent,CGColorRef,CFDictionaryRef);
    Match match=(Match)dlsym(RTLD_DEFAULT,"CGColorCreateCopyByMatchingToColorSpace");
    return match ? match(space,intent,color,options) : NULL;
}

static inline BOOL SFCIIsGenericXYZ(CFStringRef name) {
    const CFStringRef *value=(const CFStringRef *)dlsym(RTLD_DEFAULT,"kCGColorSpaceGenericXYZ");
    return value && *value && CFEqual(name,*value);
}
static inline BOOL SFCIIsColorSpace(CFTypeRef value) {
    typedef CFTypeID (*TypeID)(void);
    TypeID typeID=(TypeID)dlsym(RTLD_DEFAULT,"CGColorSpaceGetTypeID");
    if (typeID) return value && CFGetTypeID(value)==typeID();
    /* PD's public CG wrapper returns real O2ColorSpace NSObject instances. */
    Class provider=NSClassFromString(@"O2ColorSpace");
    return value && provider && [(id)value isKindOfClass:provider];
}
