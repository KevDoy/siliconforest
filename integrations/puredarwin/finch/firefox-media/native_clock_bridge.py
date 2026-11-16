# SPDX-License-Identifier: MIT
"""CGL's C CLOCK_MONOTONIC -> epoch bridge, not Python's monotonic epoch."""
import ctypes
import os
import subprocess
import time


def summarize(samples):
    if not samples:raise ValueError('No actual bracketed clock observations')
    intervals=[]
    for s in samples:
        before,after,native=s['epoch_before_ms'],s['epoch_after_ms'],s['native_monotonic_ms']
        import math
        if not all(math.isfinite(x) for x in [before,after,native]) or after<before or native<0:
            raise ValueError('Invalid clock bracket')
        intervals.append((before-native,after-native))
    lower=max(x[0] for x in intervals);upper=min(x[1] for x in intervals)
    if lower>upper:raise ValueError('No stable shared clock-offset interval')
    return dict(epoch_minus_native_ms=(lower+upper)/2,uncertainty_ms=(upper-lower)/2,
                samples=samples,scope='CGL clock_gettime(CLOCK_MONOTONIC), not Python time.monotonic; no GPU or process observation')


def capture():
    if os.uname().sysname!='Darwin':raise ValueError('Native SiliconForest clock bridge only')
    class Timespec(ctypes.Structure):_fields_=[('sec',ctypes.c_long),('nsec',ctypes.c_long)]
    library=ctypes.CDLL('/usr/lib/libSystem.B.dylib',use_errno=True)
    clock=library.clock_gettime;clock.argtypes=[ctypes.c_int,ctypes.POINTER(Timespec)];clock.restype=ctypes.c_int
    samples=[]
    for _ in range(7):
        stamp=Timespec();before=time.time_ns()/1e6
        if clock(int(time.CLOCK_MONOTONIC),ctypes.byref(stamp)):raise OSError(ctypes.get_errno(),'Native monotonic query')
        after=time.time_ns()/1e6
        if not 0<=stamp.nsec<10**9:raise ValueError('Invalid native timespec')
        samples.append(dict(epoch_before_ms=before,epoch_after_ms=after,
                            native_monotonic_ms=stamp.sec*1000+stamp.nsec/1e6))
    result=summarize(samples);result.update(clock_id=int(time.CLOCK_MONOTONIC),
        kernel_uuid=subprocess.check_output(['sysctl','-n','kern.kernelcacheuuid'],text=True).strip(),
        boot_time=subprocess.check_output(['sysctl','-n','kern.boottime'],text=True).strip())
    return result


if __name__=='__main__':
    import json
    print(json.dumps(capture()))
