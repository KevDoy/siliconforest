#!/bin/bash
# SPDX-License-Identifier: MIT
# The lines of a boot's console that matter for PDAopAudio. Usage: mic_summary.sh <console.bin>
strings -n 4 "${1:?console.bin}" |
    grep -a 'PDAopAudio:\|PDAop:\|PDSoc:\|PDDartFixed:\|^MIC_\|^PDMIC \|pdmic\|kern\.pd_aop\|panic\|Panic\|SError\|records \(in\|out\)\|bytes transferred' |
    cut -c1-420 | head -${LINES_MAX:-600}
