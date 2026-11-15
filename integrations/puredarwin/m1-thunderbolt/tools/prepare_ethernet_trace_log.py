#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Keep finite I210 diagnostics visible without global FreeBSD debug logging."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
p=root/'src/Kernel/Extensions/PDFBSDem/dev/e1000/igb_txrx.c'
s=p.read_text()
def replace(old,new):
    global s
    if s.count(old)!=1:
        raise SystemExit('I210 trace-log source drift: '+old[:80])
    s=s.replace(old,new)
if s.count('printf("PDI210TRACE ')!=4:
    raise SystemExit('I210 trace-log requires four original trace sites')
s = s.replace('printf("PDI210TRACE ', 'pd_i210_trace_log("PDI210TRACE ')
assert s.count('pd_i210_trace_log("PDI210TRACE ') == 4
p.write_text(s)

# The IOKit/FreeBSD handoff precedes descriptor generation. Record bounded
# admission and results here too, so an absent encap is not blamed on DMA.
shim = root / 'src/Kernel/fbsdkpi'
for relative, include, before, after in [
    ('xnu/PDFBSDEthernet.cpp', '#include "pd_fbsd_attach.h"',
     '\tint error = pd_fbsd_transmit(fIfp, fTxBuffer, len);',
     '''\tint error = pd_fbsd_transmit(fIfp, fTxBuffer, len);
    static uint32_t pd_trace_output;
    if (pd_i210_trace_take(&pd_trace_output))
        pd_i210_trace_log("PDI210TRACE output len=%u error=%d enabled=%u\\n",
            (unsigned)len, error, (unsigned)fEnabled);'''),
    ('pd/pd_driver.c', '#include "pd_fbsd_attach.h"',
     '\tIFF_UNLOCKGIANT(ifp);\n\treturn result;',
     '''\tIFF_UNLOCKGIANT(ifp);
    static uint32_t pd_trace_transmit;
    if (pd_i210_trace_take(&pd_trace_transmit))
        pd_i210_trace_log("PDI210TRACE transmit len=%u result=%d flags=%x drvflags=%x\\n",
            (unsigned)len, result, (unsigned)ifp->if_flags,
            (unsigned)ifp->if_drv_flags);
\treturn result;'''),
]:
    p = shim / relative
    s = p.read_text()
    replace(include, include + '\n#include "pd_i210_trace.h"')
    replace(before, after)
    p.write_text(s)
