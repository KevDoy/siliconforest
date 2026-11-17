#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Select FXPERF2 CGL 0303 and damage/cache; plan unless --apply.

Run on the M1 under gui/core/bench locks. --off disables both launcher flags;
--rollback additionally restores the previous OpenGL inode's saved bytes.
Existing applications are never signalled. No render-thread switch is added.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import tempfile

ROOT = Path('/usr/local/lib/siliconforest/firefox/160-system1')
OPENGL = Path('/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL')
BACKUP = ROOT / 'maintenance-damage-path-v4'
LAUNCHER = Path('/Applications/Firefox.app/Contents/MacOS/firefox-sf')
CANDIDATE_SHA = 'c4448fe9d989186a6196c2af498681c8458d61dc1c143173aa9524586b72ab36'
BASELINE_SHA = '52bff2c2dff1cce60362f0e21cd6346a895605ae4f3230087e2cdccb6cc793c0'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def image_uuid(data):
    if data[:4] != b'\xcf\xfa\xed\xfe':
        raise ValueError('Expected a thin little-endian 64-bit Mach-O')
    import uuid
    offset = 32
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, size = struct.unpack_from('<II', data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError('Invalid Mach-O load command')
        if command == 0x1b:
            return str(uuid.UUID(bytes=data[offset+8:offset+24])).upper()
        offset += size
    raise ValueError('Missing LC_UUID')


def atomic(path, data, mode):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.damage-', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.chmod(mode)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--apply', action='store_true')
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument('--off', action='store_true')
    actions.add_argument('--rollback', action='store_true')
    args = parser.parse_args()
    if os.uname().sysname != 'Darwin':
        raise RuntimeError('Target-only installer')
    with (ROOT / '.graphics-maintenance.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        manifest_path = ROOT / 'release.json'
        before_launcher = LAUNCHER.read_bytes()
        before = manifest_path.read_bytes()
        manifest = json.loads(before)
        if any(manifest.get(key) is not True for key in
               ['system_layout', 'system_ui', 'system_gpu_providers']):
            raise ValueError('Expected the installed canonical UI/graphics cohort')
        if manifest.get('target') != str(ROOT) or manifest.get('diagnostics'):
            raise ValueError('Unexpected installed runtime')
        if OPENGL.is_symlink() or not OPENGL.is_file():
            raise ValueError('Expected canonical regular OpenGL file')
        if args.off or args.rollback:
            receipt = json.loads((BACKUP / 'receipt.json').read_text())
            if sha(before_launcher) != receipt['selected_launcher_sha256']:
                raise ValueError('Launcher changed since promotion')
            if image_uuid(OPENGL.read_bytes()) != receipt['candidate_uuid']:
                raise ValueError('OpenGL changed since this promotion')
            for key, value in json.loads((BACKUP / 'release.json').read_bytes()).items():
                if key not in ['gpu_damage', 'gpu_image_cache'] and manifest.get(key) != value:
                    raise ValueError('Unrelated runtime selection changed: ' + key)
            updated = dict(manifest, gpu_damage=False, gpu_image_cache=False)
            print(json.dumps(dict(action='rollback' if args.rollback else 'off', apply=args.apply,
                                  damage=False, image_cache=False, backup=str(BACKUP))))
            if args.apply:
                if args.rollback:
                    old = (BACKUP / 'OpenGL').read_bytes()
                    if sha(old) != receipt['previous_opengl_sha256']:
                        raise ValueError('Rollback file changed')
                    atomic(OPENGL, old, receipt['opengl_mode'])
                    atomic(manifest_path, (BACKUP / 'release.json').read_bytes(), 0o600)
                    atomic(LAUNCHER, (BACKUP / 'firefox-sf').read_bytes(), 0o755)
                else:
                    atomic(manifest_path, (json.dumps(updated, indent=2)+'\n').encode(), 0o600)
            return
        if args.source is None or args.source.parent.parent != Path('/private/var/db/sffx'):
            raise ValueError('Expected OpenGL inside a fresh owned cold staging directory')
        cold = json.loads((args.source.parent / 'cold-verified.json').read_text())
        candidate = args.source.read_bytes()
        if (cold.get('cold_before_execution') is not True or
                cold.get('destination') != str(args.source.parent) or
                cold.get('sha256', {}).get('OpenGL') != CANDIDATE_SHA or sha(candidate) != CANDIDATE_SHA):
            raise ValueError('FXPERF2 rb2 cold identity differs')
        candidate_launcher = (args.source.parent / 'daily-launch.py').read_bytes()
        if sha(candidate_launcher) != cold.get('sha256', {}).get('daily-launch.py'):
            raise ValueError('Cold launcher identity differs')
        compile(candidate_launcher, 'firefox-sf', 'exec')
        previous_opengl = OPENGL.read_bytes()
        if sha(previous_opengl) != BASELINE_SHA or BACKUP.exists():
            raise ValueError('Expected unchanged baseline and unused rollback directory')
        updated = dict(manifest, gpu_damage=True, gpu_image_cache=True)
        after = (json.dumps(updated, indent=2)+'\n').encode()
        receipt = dict(installed_utc=datetime.now(timezone.utc).isoformat(),
                       candidate_cold_sha256=sha(candidate), candidate_uuid=image_uuid(candidate),
                       previous_opengl_sha256=sha(previous_opengl), previous_uuid=image_uuid(previous_opengl),
                       previous_manifest_sha256=sha(before), selected_manifest_sha256=sha(after),
                       previous_launcher_sha256=sha(before_launcher), selected_launcher_sha256=sha(candidate_launcher),
                       opengl_mode=OPENGL.stat().st_mode & 0o777, source=str(args.source),
                       render_thread_enabled=False, owner_processes_signalled=False)
        print(json.dumps(dict(receipt, apply=args.apply), indent=2))
        if not args.apply:
            return
        BACKUP.mkdir()
        atomic(BACKUP / 'OpenGL', previous_opengl, receipt['opengl_mode'])
        atomic(BACKUP / 'release.json', before, 0o600)
        atomic(BACKUP / 'firefox-sf', before_launcher, 0o755)
        shutil.copy2(Path(__file__), BACKUP / 'install-damage-path.py')
        atomic(BACKUP / 'receipt.json', (json.dumps(receipt, indent=2)+'\n').encode(), 0o600)
        try:
            if (manifest_path.read_bytes() != before or OPENGL.read_bytes() != previous_opengl
                    or LAUNCHER.read_bytes() != before_launcher):
                raise RuntimeError('Selection changed while preparing promotion')
            atomic(OPENGL, candidate, receipt['opengl_mode'])
            # Verify canonical bytes on M1 before any new application can execute them.
            if sha(OPENGL.read_bytes()) != CANDIDATE_SHA:
                raise RuntimeError('Canonical cold verification failed')
            atomic(LAUNCHER, candidate_launcher, 0o755)
            atomic(manifest_path, after, 0o600)
        except BaseException:
            atomic(manifest_path, before, 0o600)
            atomic(LAUNCHER, before_launcher, 0o755)
            atomic(OPENGL, previous_opengl, receipt['opengl_mode'])
            raise


if __name__ == '__main__':
    main()
