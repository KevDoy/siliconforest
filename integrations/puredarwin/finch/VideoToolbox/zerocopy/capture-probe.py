#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Make a GPU-storage/held-frame control from our existing public VT decoder.

Only the test client changes. Decoder hashes keep their existing format. This
requires actual GPU-backed output; it is not a CPU-copy performance benchmark.
"""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
def change(old,new):
 global s
 if s.count(old)!=1:raise ValueError('decoder fixture anchor changed: '+old[:80])
 s=s.replace(old,new)
change('#include <time.h>','#include <time.h>\n#include <dlfcn.h>\n#include <unistd.h>\n#include <IOSurface/IOSurface.h>')
change('static int64_t g_last_pts = INT64_MIN;', '''static int64_t g_last_pts = INT64_MIN;
static CVPixelBufferRef held;
static char heldHash[33];
static unsigned gpuFrames,heldChecks;
static void visibleHash(CVPixelBufferRef image,char hex[33]) {
 struct pd_md5 md5;pd_md5_init(&md5);
 size_t width=CVPixelBufferGetWidth(image),height=CVPixelBufferGetHeight(image);
 CVPixelBufferLockBaseAddress(image,kCVPixelBufferLock_ReadOnly);
 for(unsigned plane=0;plane<2;plane++){
  const uint8_t *data=CVPixelBufferGetBaseAddressOfPlane(image,plane);
  size_t stride=CVPixelBufferGetBytesPerRowOfPlane(image,plane);
  size_t rows=plane?(height+1)/2:height,bytes=plane?(width+1)&~(size_t)1:width;
  for(size_t row=0;row<rows;row++)pd_md5_update(&md5,data+row*stride,bytes);
 }
 CVPixelBufferUnlockBaseAddress(image,kCVPixelBufferLock_ReadOnly);pd_md5_final(&md5,hex);
}''')
change('    // the refCon of each sample is its own presentation time:', '''    IOSurfaceRef surface=CVPixelBufferGetIOSurface(image);
    int (*copyFD)(IOSurfaceRef)=dlsym(RTLD_DEFAULT,"IOSurfaceSFGPUCopyFD");
    size_t (*offset)(IOSurfaceRef)=dlsym(RTLD_DEFAULT,"IOSurfaceSFGPUGetOffset");
    int fd=surface&&copyFD?copyFD(surface):-1;
    int valid=fd>=0&&offset&&offset(surface)==16384&&IOSurfaceGetPlaneCount(surface)==2;
    if(fd>=0)close(fd);
    if(!valid){puts("CAPTURE_FAIL actual GPU-backed two-plane surface required");g_errors++;return;}
    gpuFrames++;
    // the refCon of each sample is its own presentation time:''')
change('    g_frames++;', '''    if(!held&&!g_frames){held=CVPixelBufferRetain(image);memcpy(heldHash,hex,sizeof(heldHash));}
    if(held&&g_frames==64){
        char hash[33];visibleHash(held,hash);heldChecks++;
        if(strcmp(hash,heldHash)){puts("CAPTURE_FAIL held picture overwritten");g_errors++;}
        else puts("CAPTURE_HELD stable-after-64-frames=1");
        CVPixelBufferRelease(held);held=NULL;
    }
    g_frames++;''')
change('    err = VTDecompressionSessionCreate(NULL, format, spec, attributes, &record, &session);', '''    const CFStringRef *global=dlsym(RTLD_DEFAULT,"kIOSurfaceIsGlobal");
    if(!global||!*global){puts("CAPTURE_FAIL missing global surface key");return 1;}
    CFMutableDictionaryRef surfaceProperties=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(surfaceProperties,*global,kCFBooleanTrue);
    CFDictionarySetValue(attributes,kCVPixelBufferIOSurfacePropertiesKey,surfaceProperties);
    CFRelease(surfaceProperties);
    err = VTDecompressionSessionCreate(NULL, format, spec, attributes, &record, &session);''')
change('    return g_errors || g_frames == 0 ? 1 : 0;', '''    if(held){CVPixelBufferRelease(held);held=NULL;}
    printf("CAPTURE_SUMMARY GPU-frames=%u held-checks=%u errors=%llu\\n",gpuFrames,heldChecks,(unsigned long long)g_errors);
    return g_errors || g_frames<65 || gpuFrames!=g_frames || heldChecks!=1 ? 1 : 0;''')
p.write_text(s)
