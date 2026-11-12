#!/bin/bash
# SPDX-License-Identifier: MIT
# One bench slot around a boot that starts the controller: Linux baseline (read-only),
# the boot, Linux again, the verdict. Run it through the bench so nothing else boots in
# between:   BENCH_WHO=storage tools/bench.sh run .../nv_slot.sh <name> <level> <expected write_refused>
# The ranges are those of runs/m1-storage/linux-a/ranges (fixed, so every set is comparable).
set -uo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
N=${1:?name}; LEVEL=${2:?level}; EXPECT=${3:?expected write_refused}
R=runs/m1-storage/linux-a/ranges; [ -s "$R" ] || { echo "no $R" >&2; exit 2; }
[ ! -e runs/m1-storage/STOP ] || { echo "runs/m1-storage/STOP exists: $(cat runs/m1-storage/STOP)" >&2; exit 3; }
stop() { echo "$1" | tee runs/m1-storage/STOP; exit 3; }
"$HERE/tools/linux_m1.sh" all "$N-before" "$R" || exit 2
[ "$(grep -c '^NVT_HASH [0-9]* [0-9]* [0-9a-f]\{64\}$' "runs/m1-storage/$N-before/hashes")" = "$(grep -c . "$R")" ] || { echo "the Linux baseline is incomplete; not booting" >&2; exit 2; }
NV_RANGES=$R "$HERE/tools/nv_boot.sh" "$N" "$LEVEL"
grep -q '"status": *"DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED"' "runs/m1-desktop-010/pcie-enum-attempt-$N/result.json" 2>/dev/null ||
  stop "$N: the cycle did not end DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED; all storage boots stopped"
"$HERE/tools/linux_m1.sh" all "$N-after" "$R" || stop "$N: Linux could not be read after the boot; all storage boots stopped"
python3 -B "$HERE/tools/nv_verdict.py" "runs/m1-storage/$N-before" "runs/m1-storage/$N-after" "runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin" "$EXPECT" | tee "runs/m1-storage/$N-verdict.txt"
