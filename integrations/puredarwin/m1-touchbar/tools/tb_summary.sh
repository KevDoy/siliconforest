#!/bin/bash
# SPDX-License-Identifier: MIT
# The lines of a boot's console that matter for PDAppleTouchBar. Usage: tb_summary.sh <console.bin>
strings -n 4 "${1:?console.bin}" |
    grep -a 'PDTouchBar:\|PDAdp:\|PDZ2:\|PDSpi:\|PDSoc:\|PDDartFixed:\|PDGpio:\|^TB_\|^TBTEST\|^TBCRC\|TBKEY\|panic\|Panic\|SError\|hw\.\(ncpu\|activecpu\)' |
    cut -c1-900 | head -${LINES_MAX:-400}
