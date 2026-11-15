#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Port Finch encoder/pixel sessions; retain the installed PD decoder ABI/behavior."""
from pathlib import Path
import re,runpy,sys
root=Path(sys.argv[1]);types=Path(sys.argv[2])
parse=runpy.run_path(sys.argv[3])['definitions']
files=['VTSession.c','VTCompression.c','VTPixelSessions.c']
# The declarations come from implementation signatures; constants from Finch's
# source. No SDK/header extraction. Retained decoder declarations stay retained.
constants=(root/'VTConstants.c').read_text()
# VLC imports these four public constants even for progressive video. Their
# values come from Finch's own generated source, not an Apple header extraction.
generated=(root/'VideoToolboxConstants_Generated.m').read_text()
for name in ['kVTDecompressionPropertyKey_DeinterlaceMode','kVTDecompressionPropertyKey_FieldMode',
             'kVTDecompressionProperty_DeinterlaceMode_Temporal','kVTDecompressionProperty_FieldMode_DeinterlaceFields']:
 match=re.search(r'\b'+name+r' = CFSTR\("([^"]+)"\);',generated);assert match,name
 constants+='\nconst CFStringRef '+name+' = CFSTR("'+match[1]+'");\n'
names=set(re.findall(r'const CFStringRef\s+(kVT\w+)\s*=',constants))
names.update(re.findall(r'VT_PROFILE\((kVT\w+),',constants))
header=types.read_text()
header+='\n'.join('extern const CFStringRef '+name+';' for name in sorted(names))+'\n'
for file in files:
 for ret,name,args in parse((root/file).read_text()):
  if name.startswith('VT'):header+=ret+' '+name+'('+args+');\n'
header+='\n#ifdef __cplusplus\n}\n#endif\n'
(root/'headers/VideoToolbox').mkdir(parents=True,exist_ok=True)
(root/'headers/VideoToolbox/VideoToolbox.h').write_text('#pragma once\n'+header)
# Use PD's actual CF runtime ABI, also for C++ codec compilation.
p=root/'VTInternal.h';s=p.read_text();a=s.index('typedef struct {');b=s.index('typedef enum {',a)
s=s[:a]+'#include <CoreFoundation/CFRuntime.h>\ntypedef CFRuntimeBase VTCFRuntimeBase;\ntypedef CFRuntimeClass VTCFRuntimeClass;\n\n'+s[b:];p.write_text(s)
# The original PD decompression implementation is retained in VideoToolbox.c.
# No Finch decompression object can be constructed or assigned its type ID.
p=root/'VTSession.c';s=p.read_text()
a=s.index('static void\nfinalize_decompression');b=s.index('static void\nfinalize_compression',a);s=s[:a]+s[b:]
s=s.replace('NULL, NULL, finalize_decompression,','NULL, NULL, NULL,')
s=s.replace('    if (kind < VTKindPixelTransfer || kind > VTKindCompression)','    if (kind == VTKindDecompression || kind < VTKindPixelTransfer || kind > VTKindCompression)')
a=s.index('CFTypeID\nVTDecompressionSessionGetTypeID');b=s.index('CFTypeID\nVTCompressionSessionGetTypeID',a);s=s[:a]+s[b:]
s=s.replace('    CFTypeID type = CFGetTypeID(object);','    CFTypeID type = CFGetTypeID(object);\n    if (!type) return NULL;')
route=['VTSessionSetProperty','VTSessionCopyProperty','VTSessionCopySupportedPropertyDictionary']
for name in route:s=re.sub(r'\b'+name+r'\b','SFFinch'+name,s)
p.write_text(s)
p=root/'VideoToolbox.c';s=p.read_text()
# No deinterlacer is implemented: a newly loadable client must receive the
# real unsupported error rather than an ignored processing request.
anchor='    *sessionOut = NULL;\n'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'''    if (specification && (CFDictionaryContainsKey(specification, CFSTR("FieldMode")) ||
                          CFDictionaryContainsKey(specification, CFSTR("DeinterlaceMode"))))
        return kVTPropertyNotSupportedErr;
''')
for name in route:s=re.sub(r'\b'+name+r'\b','SFDecoder'+name,s)
for name in ['VTCompressionSessionCreate','VTCompressionSessionEncodeFrame','VTCompressionSessionCompleteFrames','VTCompressionSessionInvalidate']:
 s=re.sub(r'\b'+name+r'\b','SFUnsupported'+name,s)
p.write_text(s)
# Existing constant values remain unchanged. Add only symbols not already supplied.
existing=set(re.findall(r'KEY\((kVT\w+)\)',s))
existing.update(Path(sys.argv[4]).read_text().split())
for name in existing:
 constants=re.sub(r'^const CFStringRef '+name+r'\s*=.*;\n','',constants,flags=re.M)
 constants=re.sub(r'^VT_PROFILE\('+name+r',.*;\n','',constants,flags=re.M)
(root/'VTExtraConstants.c').write_text(constants)
# Remove only the optional absent-encoder/pixel entry points now implemented.
p=root/'VTSupplement.c';s=p.read_text()
implemented={name for file in files for _,name,_ in parse((root/file).read_text()) if name.startswith('VT')}
for name in implemented:
 match=re.search(r'(?m)^(?:OSStatus|void|CVPixelBufferPoolRef)\s+'+name+r'\s*\(',s)
 if not match:continue
 begin=s.index('{',match.end());end=begin+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 s=s[:match.start()]+s[end:]
p.write_text(s)

# Bound public inputs before upstream bitrate products and buffer allocations.
p=root/'VTCompression.c';s=p.read_text().replace('if (width <= 0 || height <= 0 || (width & 1) || (height & 1))','if (width <= 0 || height <= 0 || width > 8192 || height > 8192 || (width & 1) || (height & 1))');p.write_text(s)
