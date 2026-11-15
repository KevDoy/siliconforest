#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Add native select readiness to the existing persistent raw HID queues."""
import pathlib
import sys

p = pathlib.Path(sys.argv[1]) / 'src/Kernel/Extensions/IOHIDFamily/IOHIDSystem/PDHIDEventQueue.cpp'
s = p.read_text()

def replace(old, new):
    global s
    if s.count(old) != 1:
        raise SystemExit('HID select source shape changed: ' + old[:80])
    s = s.replace(old, new)

replace('#include <sys/uio.h>', '#include <sys/uio.h>\n#include <sys/select.h>\n#include <sys/file.h>\n#include <sys/filio.h>')
for queue in ('Kbd', 'Mouse'):
    replace(f'static IOLock *g{queue}Lock;', f'static IOLock *g{queue}Lock;\nstatic struct selinfo g{queue}Select;')

functions = '''// The queue lock covers BOTH the readiness test and selrecord, matching
// the producer's queue update. A producer wakes selectors after publication.
// Nodes/locks/selinfo persist for kernel lifetime; no unload is introduced.
'''
for queue, name in [('Kbd', 'kbd'), ('Mouse', 'mouse')]:
    functions += f'''// FIONREAD also lets a consumer detect this readiness implementation on
// older kernels whose raw nodes reject every ioctl. It consumes no events.
static int
pd_hid_{name}_ioctl(dev_t, u_long command, caddr_t data, int, struct proc *)
{{
    if (command != FIONREAD) return ENOTTY;
    if (!g{queue}Lock) return ENXIO;
    IOLockLock(g{queue}Lock);
    *(int *)data = (int)((g{queue}WriteIndex - g{queue}ReadIndex) * sizeof(g{queue}Events[0]));
    IOLockUnlock(g{queue}Lock);
    return 0;
}}

static int
pd_hid_{name}_select(dev_t, int which, void *wql, struct proc *p)
{{
    if (!g{queue}Lock || which != FREAD) return 0;
    IOLockLock(g{queue}Lock);
    bool ready = g{queue}ReadIndex != g{queue}WriteIndex;
    if (!ready) selrecord(p, &g{queue}Select, wql);
    IOLockUnlock(g{queue}Lock);
    return ready ? 1 : 0;
}}

'''
replace('static struct cdevsw pd_hid_kbd_cdevsw = {', functions + 'static struct cdevsw pd_hid_kbd_cdevsw = {')
for name in ('kbd', 'mouse'):
    opening = f'static struct cdevsw pd_hid_{name}_cdevsw = {{'
    start = s.index(opening)
    end = s.index('\n};', start)
    part = s[start:end]
    if part.count('eno_select') != 1 or part.count('eno_ioctl') != 1:
        raise SystemExit('HID select cdevsw changed')
    part = part.replace('eno_select', f'pd_hid_{name}_select').replace('eno_ioctl', f'pd_hid_{name}_ioctl')
    s = s[:start] + part + s[end:]
replace('    kick = pd_hid_console_key(usage, down);\n    IOLockUnlock(gKbdLock);',
        '    kick = pd_hid_console_key(usage, down);\n    IOLockUnlock(gKbdLock);\n    selwakeup(&gKbdSelect);')
replace('        gMouseReadIndex = gMouseWriteIndex - (UInt32)(sizeof(gMouseEvents) / sizeof(gMouseEvents[0]));\n    IOLockUnlock(gMouseLock);',
        '        gMouseReadIndex = gMouseWriteIndex - (UInt32)(sizeof(gMouseEvents) / sizeof(gMouseEvents[0]));\n    IOLockUnlock(gMouseLock);\n    selwakeup(&gMouseSelect);')
p.write_text(s)
