#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Qualify movie decode identity and the measured headless HAL-clock run."""
import argparse
import json
from pathlib import Path
import re


def frames(text, prefix):
    pattern = r'^'+prefix+r' frame=(\d+) pts=([\d.]+).*?md5=([a-f0-9]{32})$'
    return [(int(n), float(pts), digest) for n, pts, digest in re.findall(pattern, text, re.M)]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('candidate', type=Path)
    ap.add_argument('reference', type=Path)
    ap.add_argument('linux_framemd5', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    candidate = {r['name']: r for r in json.loads(a.candidate.read_text())}
    reference = {r['name']: r for r in json.loads(a.reference.read_text())}
    decode, play = candidate['movie-decode-only'], candidate['movie-headless']
    native = reference['movie-decode-only']
    decoded = frames(decode['stdout'], 'MOVIE_DECODE')
    presented = frames(play['stdout'], 'MOVIE_HEADLESS_DELIVER')
    expected = frames(native['stdout'], 'MOVIE_DECODE')
    linux = [line.split(',')[-1].strip() for line in a.linux_framemd5.read_text().splitlines()
             if line and not line.startswith('#')]
    matches = re.findall(r'^MOVIE_SYNC (.*)$', play['stdout'], re.M)
    sync = dict(item.split('=', 1) for item in matches[0].split()) if len(matches) == 1 else {}
    checks = dict(
        programs_completed=all(r['status'] == 'OK' and r['returncode'] == 0 for r in [decode, play, native]),
        same_linux_built_program=decode['sha256'] == play['sha256'] == native['sha256'],
        all_frames=len(decoded) == len(presented) == len(expected) == len(linux) == 900,
        ordered_indices=[r[0] for r in presented] == list(range(900)),
        ordered_pts=all(x[1] < y[1] for x, y in zip(presented, presented[1:])),
        same_pts_pixels=decoded == presented == expected,
        linux_pixels=[r[2] for r in decoded] == linux,
        hardware_reported=sync.get('hardware') == '1',
        all_audio_completed=sync.get('audio_queued') == sync.get('audio_completed') == '1440000',
        audio_rate=sync.get('rate') == '48000',
        headless_only=sync.get('mode') == 'headless',
        thirty_seconds=abs(float(sync.get('wall', 'inf')) - 30) < 1,
        clock_drift=abs(float(sync.get('clock_wall_drift_ms', 'inf'))) < 100,
        delivery_lateness=float(sync.get('max_delivery_late_ms', 'inf')) < 100)
    result = dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks, measured=sync,
                  note='Queue sample clock measures frames consumed by the retained PD HAL IOProc. '
                  'Headless delivery is not layer presentation, panel scanout, acoustic output, '
                  'or proof of a general streaming player. One generated 30-second fixture.')
    a.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
    return int(not all(checks.values()))


if __name__ == '__main__':
    raise SystemExit(main())
