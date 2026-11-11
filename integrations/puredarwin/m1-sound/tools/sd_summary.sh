#!/bin/bash
# SPDX-License-Identifier: MIT
# The lines of a boot's console that matter for PDAppleSound. Usage: sd_summary.sh <console.bin>
strings -n 4 "${1:?console.bin}" |
    grep -a 'PDSound:\|PDSoc:\|PDI2C:\|PDCs42l83:\|PDDartFixed:\|PDMca:\|PDGpio:\|^SD_\|pdplay:\|pdaudio\|kern\.pd_sound\|panic\|Panic\|SError\|records \(in\|out\)\|bytes transferred' |
    cut -c1-420 | head -${LINES_MAX:-400}
