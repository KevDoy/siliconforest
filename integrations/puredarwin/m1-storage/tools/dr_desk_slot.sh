#!/bin/bash
# SPDX-License-Identifier: MIT
# One bench slot around a desktop boot rooted on the SiliconForest partition (dr_desk_boot.sh):
# Linux baseline, the boot, Linux again, the verdict of nv_verdict.py in root mode.
#   BENCH_WHO=disk tools/bench.sh run .../dr_desk_slot.sh <name> <hv|native> [upper bound s]
# DR_RW=1 (step 3d): the root read-write through the write-fenced build; afterwards Linux also
# checks our partition (dr_linux_root.sh wcheck) and the verdict runs in its write mode for a
# root (NV_WMODE=root). NV_PREV_CONSOLE=<console of the previous read-write boot>: the files and
# the desktop state that boot wrote must be found by this one.
# DR_HALT="<command>" (dr_desk_boot.sh): the boot is ended by the system's own command and the machine
# restarts itself; the verdict runs with NV_WMODE=halt, and the slot stops unless the controller sent NO
# reset. The desktop-state and clock checks of that mode are nv_verdict.py's.
set -uo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
N=${1:?name}; MODE=${2:?hv or native}; LIMIT=${3:-200}
R=${NV_RANGE_FILE:-runs/m1-storage/linux-c/ranges}; [ -s "$R" ] || { echo "no $R" >&2; exit 2; }
[ ! -e runs/m1-storage/STOP ] || { echo "runs/m1-storage/STOP exists: $(cat runs/m1-storage/STOP)" >&2; exit 3; }
stop() { echo "$1" | tee runs/m1-storage/STOP; exit 3; }
. private/disk-root/sf-part.env || exit 2
"$HERE/tools/linux_m1.sh" all "$N-before" "$R" || exit 2
[ "$(grep -c '^NVT_HASH [0-9]* [0-9]* [0-9a-f]\{64\}$' "runs/m1-storage/$N-before/hashes")" = "$(grep -c . "$R")" ] || { echo "the Linux baseline is incomplete; not booting" >&2; exit 2; }
DR_RANGES=$R "$HERE/tools/dr_desk_boot.sh" "$N" "$MODE" "$LIMIT"
C=runs/m1-desktop-010/pcie-enum-attempt-$N
# natively nothing is typed back to the controller the same way: its own status is checked for Linux, the rest by the verdict
grep -q '"status": *"[A-Z_]*LINUX_VERIFIED"' "$C/result.json" 2>/dev/null || stop "$N: the cycle did not end with Linux verified; all storage boots stopped"
"$HERE/tools/linux_m1.sh" all "$N-after" "$R" || stop "$N: Linux could not be read after the boot; all storage boots stopped"
if [ "${DR_RW:-0}" = 1 ]; then
  sleep 30
  export NV_WCHECK=$ROOT/runs/m1-storage/$N-wcheck.txt
  DR_OUT=$NV_WCHECK "$HERE/tools/dr_linux_root.sh" wcheck || stop "$N: our partition could not be checked from Linux after the read-write boot; all storage boots stopped"
  [ "${DR_HALT:-}" != menu ] || export NV_HALT_MENU=1
  if [ -n "${DR_HALT:-}" ]; then export NV_RESETS=$(sed -n 's/.*"reset_attempts": *\([0-9]*\).*/\1/p' "$C/result.json" | head -1); fi
  NV_VERDICT_MODE=write NV_WMODE=$([ -n "${DR_HALT:-}" ] && echo halt || echo root) NV_PART_LBA=$SF_PART_LBA NV_PART_BLOCKS=$SF_PART_BLOCKS \
    python3 -B "$HERE/tools/nv_verdict.py" "runs/m1-storage/$N-before" "runs/m1-storage/$N-after" "$C/guest/console.bin" any 3 | tee "runs/m1-storage/$N-verdict.txt"
else
NV_VERDICT_MODE=root NV_PART_LBA=$SF_PART_LBA NV_TREE_REF=${NV_TREE_REF:-$ROOT/runs/m1-storage/ref-tree-diskroot.txt} \
  python3 -B "$HERE/tools/nv_verdict.py" "runs/m1-storage/$N-before" "runs/m1-storage/$N-after" "$C/guest/console.bin" any 3 | tee "runs/m1-storage/$N-verdict.txt"
fi
strings -n 4 "$C/guest/console.bin" | grep -a 'GPUDESK\|WMSTAT\|NV_EARLY\|WMSCALE\|WMSET\|WMAUTO\|SF_\|NV_CLOCK\|NV_DESKSTATE\|NV_LEFT\|NV_SF' | sed 's/[0-9A-Fa-f]\{8\}-[0-9A-Fa-f-]\{27\}/GUID/g' | cut -c1-260 | head -150 | tee "runs/m1-storage/$N-desktop.txt"
