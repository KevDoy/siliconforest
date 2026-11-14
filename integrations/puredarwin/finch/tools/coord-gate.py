#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read-only local SSH hold gate. A quoted completion marker never clears a hold."""
import argparse
from pathlib import Path
import re


def pending_hold(text):
    pending = None
    markers = []
    for line in text.splitlines():
        # Completion must be the actual agent event, not a phrase inside a notice.
        event = re.match(r'^\S+\s+(?:VK|COCOA4)[: ]+(.+)$', line)
        if pending and event and any(event[1].startswith(marker) for marker in markers):
            pending = None
            markers = []
        lower = line.lower()
        if event and event[1].startswith('core.lock: GUARDED INSTALL'):
            pending, markers = line, ['core install finished']
        if 'stay off' in lower and 'until' in lower:
            suffix = line[lower.index('until')+5:]
            quoted = re.findall(r"['\"]([^'\"]+)['\"]", suffix)
            selected = [m.removeprefix('VK ').removeprefix('COCOA4 ') for m in quoted
                        if any(word in m.lower() for word in ('back', 'done', 'siliconforest'))]
            if selected:
                pending, markers = line, selected
        if 'CODEX received coordinator M1 SSH hold;' in line:
            pending, markers = line, ['M1 back in SiliconForest']
    return pending


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log', type=Path, default=Path('runs/m1-coord/log.txt'))
    args = ap.parse_args()
    pending = pending_hold(args.log.read_text())
    if pending:
        print('M1 SSH hold active; no connection attempted. '+pending)
        return 77
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
