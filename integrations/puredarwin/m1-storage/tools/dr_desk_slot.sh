#!/bin/bash
# SPDX-License-Identifier: MIT
# One bench slot around a desktop boot rooted on the SiliconForest partition (dr_desk_boot.sh):
# Linux baseline, the boot, Linux again, the verdict of nv_verdict.py in root mode.
#   BENCH_WHO=disk tools/bench.sh run .../dr_desk_slot.sh <name> <hv|native> [upper bound s]
set -uo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
N=${1:?name}; MODE=${2:?hv or native}; LIMIT=${3:-200}
R=${NV_RANGE_FILE:-runs/m1-storage/linux-b/ranges}; [ -s "$R" ] || { echo "no $R" >&2; exit 2; }
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
NV_VERDICT_MODE=root NV_PART_LBA=$SF_PART_LBA NV_TREE_REF=${NV_TREE_REF:-$ROOT/runs/m1-storage/ref-tree-diskroot.txt} \
  python3 -B "$HERE/tools/nv_verdict.py" "runs/m1-storage/$N-before" "runs/m1-storage/$N-after" "$C/guest/console.bin" any 3 | tee "runs/m1-storage/$N-verdict.txt"
strings -n 4 "$C/guest/console.bin" | grep -a 'GPUDESK\|WMSTAT\|NV_EARLY' | sed 's/[0-9A-Fa-f]\{8\}-[0-9A-Fa-f-]\{27\}/GUID/g' | cut -c1-260 | head -70 | tee "runs/m1-storage/$N-desktop.txt"
