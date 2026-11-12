#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""../../chonkstep-darwin/stage_ramdisk.py, unmodified, for the RAM root made from the tip.

That stager accepts one base image by hash (the pin's qualified RAM root). This runs the same
code with the hash of the tip's base image (RB_BASE_SHA256, set by stage_ramdisk_rebase.py
from the record written when the image was extracted). Nothing else differs: same arguments,
same replacement and dump-back checks, same record.
"""
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'chonkstep-darwin'))
import stage_ramdisk  # noqa: E402

sha = os.environ.get('RB_BASE_SHA256', '')
if not re.fullmatch('[0-9a-f]{64}', sha):
    sys.exit('stage_ramdisk_tipbase.py: RB_BASE_SHA256 is not set')
stage_ramdisk.BASE_SHA = sha
stage_ramdisk.main()
