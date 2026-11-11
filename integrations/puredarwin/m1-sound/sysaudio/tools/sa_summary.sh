#!/bin/bash
# SPDX-License-Identifier: MIT
# The lines of a boot's console that matter for the system audio path. Usage: sa_summary.sh <console.bin>
strings -n 4 "${1:?console.bin}" |
    grep -a '^SA_\|coreaudiod\|pdtone\|pdvolume\|pcmplay\|PDSound: \(playing\|stopped\|codec input\|could not\)\|kern\.pd_sound\.volume_db\|pdaudio\|panic\|Panic\|SError\|dyld\|not found\|Killed\|Abort\|Segmentation\|Bus error' |
    cut -c1-420 | head -${LINES_MAX:-400}
