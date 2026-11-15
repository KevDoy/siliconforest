/* SPDX-License-Identifier: MIT */
/* Reads defaults only; the sole mutation is our own PNG inside the test packet. */
#include <MediaAccessibility/MediaAccessibility.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
int main(int argc,char **argv){
    if(argc!=2)return 2;
    CFArrayRef languages=MACaptionAppearanceCopySelectedLanguages(kMACaptionAppearanceDomainDefault);
    MACaptionAppearanceBehavior behavior=0;
    CGFloat opacity=MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainDefault,&behavior);
    int valid=languages && CFGetTypeID(languages)==CFArrayGetTypeID() && isfinite(opacity) && opacity>=0 && opacity<=1;
    if(languages)CFRelease(languages);
    CFStringRef path=MAImageCaptioningCopyMetadataTagPath();valid=valid && path && CFGetTypeID(path)==CFStringGetTypeID();if(path)CFRelease(path);
    printf("caption defaults/types/range=%d\n",valid);if(!valid)return 3;
    CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)argv[1],strlen(argv[1]),false);
    CFErrorRef error=NULL;CFStringRef text=CFSTR("SiliconForest original caption fixture");
    int wrote=MAImageCaptioningSetCaption(url,text,&error);if(error){CFRelease(error);error=NULL;}
    CFStringRef got=MAImageCaptioningCopyCaption(url,&error);int same=got && CFStringCompare(got,text,0)==kCFCompareEqualTo;
    printf("caption own-PNG write=%d roundtrip=%d error=%d\n",wrote,same,error!=NULL);
    if(got)CFRelease(got);if(error)CFRelease(error);CFRelease(url);
    return wrote && same?0:4;
}
