#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Boot a Cocoa run image in QEMU (arm64 `virt`, TCG, headless) and drive it from a script.

The machine, the QMP and serial plumbing are the QEMU stream's (qd_run.py: `qemu_command`,
`Qmp`, `Serial`, imported). What is added here is a step list, because Mac applications are
checked from outside: the pointer is a USB tablet (absolute positions, so a click lands where
the script says), keys are QMP key events, and every check is a regular expression that must
appear on the guest console (the applications print one line per observable event).

  cocoa_run.py --image runs/cocoa/<run>/disk.img --out runs/cocoa/<run> --script steps.json

steps.json is a list of steps, run in order after the session marker; the first failure ends
the run (later steps are reported as not run):
  {"wait": "REGEX", "timeout": 120}            a console line (searched after the previous match)
  {"sleep": 2.5}
  {"shot": "name"}                              runs/cocoa/<run>/shots/name.png
  {"move": [x, y]}                              pointer to framebuffer pixel x,y
  {"click": [x, y], "button": "left"}           move, press, release
  {"drag": [x0, y0, x1, y1]}
  {"key": ["ctrl", "q"]}                        one chord (QMP qcodes)
  {"type": "text"}                              letters, digits, space, a few punctuation keys
  {"expect": "REGEX", "timeout": 20, "name": "button"}   as wait, and recorded as a named check
  {"absent": "REGEX", "name": "no crash"}       the pattern must not have appeared so far
Exit 0: every step done and every check passed. 1: a check failed. 2: could not run.
result.json: status, times, per-step results, screenshots.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import threading
import time

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'qemu-dev'))
import qd_run  # noqa: E402

QCODES = {' ': 'spc', '\n': 'ret', '.': 'dot', ',': 'comma', '-': 'minus', '=': 'equal', '/': 'slash', ';': 'semicolon',
          "'": 'apostrophe', '\t': 'tab'}
SHIFTED = {':': 'semicolon', '_': 'minus', '+': 'equal', '?': 'slash', '!': '1', '@': '2', '#': '3', '$': '4', '%': '5',
           '&': '7', '*': '8', '(': '9', ')': '0', '"': 'apostrophe'}


class Driver:
    def __init__(self, qmp, ser, proc, out, size):
        self.qmp, self.ser, self.proc, self.out = qmp, ser, proc, out
        self.w, self.h = size
        self.pos = 0
        self.shots = []

    def move(self, x, y):
        # QEMU's absolute axes run 0..0x7fff over the framebuffer.
        self.qmp.cmd('input-send-event', events=[
            {'type': 'abs', 'data': {'axis': 'x', 'value': int(x * 0x7fff / max(self.w - 1, 1))}},
            {'type': 'abs', 'data': {'axis': 'y', 'value': int(y * 0x7fff / max(self.h - 1, 1))}}])
        time.sleep(0.15)

    def click(self, x, y, button='left', hold=0.12):
        self.move(x, y)
        self.move(x, y)
        self.qmp.button(button, True)
        time.sleep(hold)
        self.qmp.button(button, False)
        time.sleep(0.15)

    def drag(self, x0, y0, x1, y1, steps=12):
        self.move(x0, y0)
        self.qmp.button('left', True)
        time.sleep(0.2)
        for i in range(1, steps + 1):
            self.move(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps)
        time.sleep(0.2)
        self.qmp.button('left', False)

    def type(self, text):
        for c in text:
            if c in SHIFTED:
                self.qmp.keys('shift', SHIFTED[c])
            elif c.isupper():
                self.qmp.keys('shift', c.lower())
            else:
                self.qmp.keys(QCODES.get(c, c))
            time.sleep(0.12)

    def shot(self, name):
        path = self.out / 'shots' / (name + '.png')
        if self.qmp.shot(path):
            self.shots.append(str(path))
            return True
        return False

    def wait(self, pattern, timeout):
        # In slices, so that a session that has ended is noticed instead of waited out.
        deadline = time.time() + timeout
        while True:
            m = self.ser.wait(pattern.encode(), min(deadline, time.time() + 2), self.proc, self.pos)
            if m:
                self.pos = m.end()
                return m
            if time.time() >= deadline or self.proc.poll() is not None or b'QDDESK_END' in self.ser.text():
                return None


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--image', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--script', type=Path, required=True)
    p.add_argument('--resolution', default='1024x768')
    p.add_argument('--timeout', type=int, default=900)
    p.add_argument('--qemu', default='qemu-system-aarch64')
    p.add_argument('--cpu', default='neoverse-n2')
    p.add_argument('--smp', type=int, default=2)
    p.add_argument('--memory', type=int, default=2048)
    p.add_argument('--qemu-arg', action='append', default=[])
    a = p.parse_args()
    a.window, a.pointer = False, 'tablet'
    out = a.out.resolve()
    (out / 'shots').mkdir(parents=True, exist_ok=True)
    for stale in ('serial.sock', 'qmp.sock', 'serial.log', 'result.json'):
        (out / stale).unlink(missing_ok=True)
    steps = json.loads(a.script.read_text())
    size = tuple(int(v) for v in a.resolution.split('x'))
    t0 = time.time()
    end_by = t0 + a.timeout
    env = {k: v for k, v in os.environ.items() if k not in ('DISPLAY', 'WAYLAND_DISPLAY')}
    proc = subprocess.Popen(qd_run.qemu_command(a, out), stdin=subprocess.DEVNULL, stdout=open(out / 'qemu.log', 'wb'),
                            stderr=subprocess.STDOUT, env=env, cwd=out)
    result = dict(status='ERROR', times={}, steps=[], checks={}, host_load=os.getloadavg()[0])
    drv = None
    try:
        qmp = qd_run.Qmp(out / 'qmp.sock', t0 + 20)
        ser = qd_run.Serial(out / 'serial.sock', out / 'serial.log', t0 + 20)
        drv = Driver(qmp, ser, proc, out, size)
        # QEMU writes the console to this socket as well as to the log file; if nobody reads
        # it while a step sleeps, QEMU's main loop blocks on the write, the guest's console
        # stalls and QMP stops answering. One thread reads it for the whole run.
        def drain():
            while proc.poll() is None:
                ser.drain()
                time.sleep(0.05)
        threading.Thread(target=drain, daemon=True).start()
        if not ser.wait(rb'QDDESK_BEGIN', end_by, proc):
            raise RuntimeError('the session did not start')
        result['times']['session'] = round(time.time() - t0, 1)
        while not ser.wait(rb'QDDESK_INPUT', time.time() + 0.5, proc) and time.time() < end_by and proc.poll() is None:
            qmp.wake_input('tablet')
        ok = True
        for i, step in enumerate(steps):
            rec = dict(step=step, at=round(time.time() - t0, 1))
            if not ok or proc.poll() is not None or time.time() > end_by:
                rec['result'] = 'not run'
                result['steps'].append(rec)
                continue
            if 'wait' in step or 'expect' in step:
                pattern = step.get('wait') or step['expect']
                m = drv.wait(pattern, step.get('timeout', 60))
                rec['result'] = 'ok' if m else 'TIMEOUT'
                if m:
                    rec['line'] = m.group(0).decode(errors='replace')[:200]
                if 'expect' in step:
                    result['checks'][step.get('name', pattern)] = bool(m)
                ok = bool(m) or step.get('optional', False)
            elif 'absent' in step:
                found = re.search(step['absent'].encode(), ser.text())
                rec['result'] = 'ok' if not found else 'PRESENT'
                result['checks'][step.get('name', 'absent ' + step['absent'])] = not found
            elif 'sleep' in step:
                time.sleep(step['sleep'])
                rec['result'] = 'ok'
            elif 'shot' in step:
                rec['result'] = 'ok' if drv.shot(step['shot']) else 'no screenshot'
            elif 'move' in step:
                drv.move(*step['move'])
                rec['result'] = 'ok'
            elif 'click' in step:
                drv.click(*step['click'], button=step.get('button', 'left'))
                rec['result'] = 'ok'
            elif 'drag' in step:
                drv.drag(*step['drag'])
                rec['result'] = 'ok'
            elif 'key' in step:
                qmp.keys(*step['key'])
                rec['result'] = 'ok'
            elif 'type' in step:
                drv.type(step['type'])
                rec['result'] = 'ok'
            else:
                rec['result'] = 'unknown step'
                ok = False
            rec['done'] = round(time.time() - t0, 1)
            print('cocoa-run: %-60s %s' % (json.dumps(step)[:60], rec['result']), flush=True)
            result['steps'].append(rec)
        result['times']['end'] = round(time.time() - t0, 1)
        done = all(s['result'] == 'ok' or s['step'].get('optional') for s in result['steps'])
        result['status'] = 'PASS' if done and all(result['checks'].values()) else 'FAIL'
    except Exception as e:                                    # noqa: BLE001 - reported, then exit 2
        result['error'] = str(e)
    finally:
        try:
            if proc.poll() is None:
                qmp.cmd('quit')
        except Exception:                                     # noqa: BLE001
            pass
        try:
            proc.wait(10)
        except subprocess.TimeoutExpired:
            proc.kill()
        for s in ('serial.sock', 'qmp.sock'):
            (out / s).unlink(missing_ok=True)
    result['shots'] = drv.shots if drv else []
    (out / 'result.json').write_text(json.dumps(result, indent=1) + '\n')
    print('cocoa-run: %s  (%s)' % (result['status'], out))
    for name, passed in result['checks'].items():
        print('cocoa-run:   %-40s %s' % (name, 'ok' if passed else 'FAILED'))
    if 'error' in result:
        print('cocoa-run: ' + result['error'], file=sys.stderr)
        return 2
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
