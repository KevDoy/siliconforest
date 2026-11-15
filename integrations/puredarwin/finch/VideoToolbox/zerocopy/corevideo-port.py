#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Private one-shot buffer lifetime slot, independent of mutable attachments."""
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
a='    CVPixelBufferPoolRef pool;\n';assert s.count(a)==1
s=s.replace(a,a+'    _Atomic(CFTypeRef) sfLifetime;\n')
a='    _FinchCVBufferInit(&b->cv);\n';assert s.count(a)==1
s=s.replace(a,a+'    atomic_init(&b->sfLifetime, NULL);\n')
a='    _FinchCVBufferFinalize(&b->cv);\n';assert s.count(a)==1
s=s.replace(a,a+'    CFTypeRef lifetime = atomic_load(&b->sfLifetime);\n    if (lifetime) CFRelease(lifetime);\n')
s+='''
/* Private producer API. Set exactly once, before publication, on a non-pooled
 * IOSurface wrapper. Removing/copying public attachments cannot drop it. */
CVReturn CVPixelBufferSFSetLifetimeObject(CVPixelBufferRef b, CFTypeRef lifetime)
{
    if (!_FinchCVIsBuffer(b) || !lifetime || !b->surface || b->pool)
        return kCVReturnInvalidArgument;
    CFRetain(lifetime);
    CFTypeRef expected = NULL;
    if (!atomic_compare_exchange_strong(&b->sfLifetime, &expected, lifetime)) {
        CFRelease(lifetime);
        return kCVReturnInvalidArgument;
    }
    return kCVReturnSuccess;
}
'''
p.write_text(s)
