#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Use the shared hold parser, including actual bare core-lock releases.

Some owners record `core.lock released` without an install-result suffix. That
ends the active pipeline regardless of its success. It says nothing about a
later reservation or any other owner's hold. Never invent completion events.
"""
import importlib.util
from pathlib import Path
import re


def completed_core_starts(text):
    lines = text.splitlines()
    active = {}
    retired = set()
    for index, line in enumerate(lines):
        event = re.match(r'^\S+\s+([A-Z][A-Z0-9_-]*)[: ]+(.+)$', line)
        if not event:
            continue
        owner, body = event[1], event[2].lower()
        if re.match(r'^(?:core\.lock:\s*)?guarded(?: core)? install\b', body):
            active[owner] = index
        elif body == 'core.lock released' and owner in active:
            retired.add(active.pop(owner))
    return '\n'.join(line for index, line in enumerate(lines) if index not in retired)


def main():
    repo = Path(__file__).resolve().parents[3]
    spec = importlib.util.spec_from_file_location('shared_coord_gate',
        repo / 'integrations/puredarwin/finch/tools/coord-gate.py')
    shared = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shared)
    source = (repo / 'runs/m1-coord/log.txt').read_text()
    pending = shared.pending_hold(completed_core_starts(source))
    if pending:
        print('M1 SSH hold active; no connection attempted. ' + pending)
        return 77
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
