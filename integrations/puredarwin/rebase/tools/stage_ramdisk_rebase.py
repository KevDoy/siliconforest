#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""PCIE_STAGER for boots of the rebase target: the slim RAM root, made from the tip's root.

tools/pcie_assemble.py calls its stager with --base <the pin's RAM root> --mapping <files>
--output <image>. This one ignores that base and uses the RAM root extracted from the tip's
arm64 minimal image (rb_ramdisk_base.sh: artifacts/rebase/ramdisk-tip.img and its record),
and in the mapping it replaces libSystem, the one file the base mapping takes from a build of
the pinned tree, by the tip's (../build-libsystem.nix). Every other staged file is the same
file as on the pin (compositor, terminal, Mesa, audio server, tools, firmware). Then it runs
m1-boot-speed/tools/stage_ramdisk_slim.py as it is, with the base-hash check pointed at the
tip image (stage_ramdisk_tipbase.py). The slim stager's image cache is not used (it keeps
three images for all streams).
  RB_RAMDISK_BASE=<image>   another base (its record <image>.json beside it)
  RB_LIBSYSTEM=<dir>        another libSystem build; RB_LIBSYSTEM=mapping keeps the mapping's
"""
import hashlib
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve()
LAB = HERE.parents[4]
SLIM = LAB / 'integrations/puredarwin/m1-boot-speed/tools'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    argv = sys.argv[1:]
    base = Path(os.environ.get('RB_RAMDISK_BASE') or LAB / 'artifacts/rebase/ramdisk-tip.img')
    record = json.loads(Path(str(base) + '.json').read_text())
    if digest(base) != record['sha256']:
        sys.exit('stage_ramdisk_rebase.py: %s is not the image its record describes' % base)
    mapping_path = Path(argv[argv.index('--mapping') + 1])
    output = Path(argv[argv.index('--output') + 1]).absolute()
    mapping = json.loads(mapping_path.read_text())
    libsystem = os.environ.get('RB_LIBSYSTEM') or record['libsystem']
    for entry in mapping['files']:
        if entry['destination'] == '/usr/lib/libSystem.B.dylib':
            if libsystem != 'mapping':
                entry['source'] = str(Path(libsystem) / 'usr/lib/libSystem.B.dylib')
            # the file it replaces is the tip image's own libSystem
            entry['replaces_sha256'] = record['files']['/usr/lib/libSystem.B.dylib']
        elif 'replaces_sha256' in entry:
            entry['replaces_sha256'] = record['files'][entry['destination']]
    tip_mapping = output.with_name('mapping.tip.json')
    tip_mapping.write_text(json.dumps(mapping, indent=1) + '\n')
    argv[argv.index('--base') + 1] = str(base)
    argv[argv.index('--mapping') + 1] = str(tip_mapping)
    os.environ['RB_BASE_SHA256'] = record['sha256']
    os.environ['BS_RAMDISK_CACHE'] = '0'
    sys.path.insert(0, str(SLIM))
    import stage_ramdisk_slim as slim
    slim.STAGER = HERE.with_name('stage_ramdisk_tipbase.py')
    sys.argv = [str(SLIM / 'stage_ramdisk_slim.py')] + argv
    slim.main()


if __name__ == '__main__':
    main()
