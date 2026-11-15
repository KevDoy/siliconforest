#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read-only local SSH hold gate. A quoted completion marker never clears a hold."""
import argparse
from pathlib import Path
import re


def pending_hold(text):
    active = {}
    owners = '[A-Z][A-Z0-9_-]*'
    agent_names = set(re.findall(r'^\S+\s+('+owners+r')[: ]', text, re.M))
    for line in text.splitlines():
        # Completion must be the actual agent event, not a phrase inside a notice.
        event = re.match(r'^\S+\s+('+owners+r')[: ]+(.+)$', line)
        if event:
            owner, body = event[1], event[2]
            lower_body = body.lower()
            if lower_body.startswith('gui.lock released'):
                active.pop((owner, 'gpu-session'), None)
            elif ('announced gpu tests' in lower_body and 'did not run' in lower_body
                  and 're-announced' in lower_body):
                active.pop((owner, 'gpu-session'), None)
            elif lower_body.startswith(('gpu tests postponed', 'postponed:')):
                active.pop((owner, 'gpu-session'), None)
            elif lower_body.startswith('in ') and 'risky gpu test' in lower_body:
                active[(owner, 'gpu-session')] = (line, [], owner)
            if lower_body.startswith('cancelled ') and 'no reboot' in lower_body:
                active.pop((owner, 'explicit'), None)
                active.pop((owner, 'reboot'), None)
            for key, (notice, markers, release_owner) in list(active.items()):
                if release_owner in (None, owner) and any(body.startswith(m) for m in markers):
                    del active[key]
            if lower_body.startswith(('core install finished', 'guarded core install finished', 'guarded install finished')):
                active.pop((owner, 'core'), None)
                active.pop((owner, 'core-announced'), None)
            elif re.match(r'^(?:core\.lock:\s*)?guarded(?: core)? install\b', lower_body):
                # Actual guarded pipelines serialize on the shared core.lock.
                # A later owner's actual start supersedes a missing historical
                # end notice; future announcements must remain independent.
                for key in list(active):
                    if key[1] == 'core':
                        del active[key]
                active[(owner, 'core')] = (line, [], owner)
            elif re.match(r'^in\b.*guarded(?: core)? install\b', lower_body):
                active[(owner, 'core-announced')] = (line, [], owner)
            if (lower_body.startswith('m1 back in siliconforest') or
                (lower_body.startswith('install end ') and 'm1 back in siliconforest' in lower_body)):
                active.pop((owner, 'reboot'), None)
                active.pop((owner, 'gpu'), None)
                active.pop((owner, 'gpu-session'), None)
            elif any(phrase in lower_body for phrase in
                     ['m1 reboot', 'siliconforest rebooting', 'clean reboot to linux', 'reboot now']):
                active[(owner, 'reboot')] = (line, [], owner)
            if lower_body.startswith('risky gpu test ') and ' finished:' in lower_body:
                active.pop((owner, 'gpu'), None)
            elif lower_body.startswith('risky gpu test over ssh'):
                active[(owner, 'gpu')] = (line, [], owner)
        lower = line.lower()
        if 'stay off' in lower and 'until' in lower:
            suffix = line[lower.index('until')+5:]
            quoted = re.findall(r"['\"]([^'\"]+)['\"]", suffix)
            selected = []
            for marker in quoted:
                if not any(word in marker.lower() for word in ('back', 'done', 'siliconforest')):
                    continue
                prefix = marker.split(' ', 1)[0]
                selected.append(marker.removeprefix(prefix+' ') if prefix in agent_names else marker)
            if selected:
                owner = event[1] if event else None
                active[(owner, 'explicit')] = (line, selected, None)
        if 'CODEX received coordinator M1 SSH hold;' in line:
            active[(None, 'explicit')] = (line, ['M1 back in SiliconForest'], None)
    return '\n'.join(notice for notice, _, _ in active.values()) or None


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
