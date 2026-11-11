#!/bin/bash
# SPDX-License-Identifier: MIT
# One boot of the M1 with the PDAopAudio collection and a guest script, through
# tools/pcie_iterate.sh, which queues for the bench itself: do not wrap it. The collection
# is handed over with PCIE_KC; nothing shared is written.
# Usage: mic_boot.sh <name: sd40 .. sd59>
#   MIC_KC        collection store path (default: build ../build-kc.nix)
#   MIC_GUEST     guest script template (default guest/darwin-mic-test.in;
#                 guest/darwin-mic-listen.in is the attended one)
#   MIC_RUNTIME   upper bound of the guest's run in seconds, at most 600 (default 240)
#   MIC_BOOTARGS  kernel boot arguments besides cpus= (default "pdsound=0 pdaop_diag=1": the
#                 jack and speaker driver in the same collection stays off, and this
#                 driver logs pin and power states around the first capture; pdaop=,
#                 pdaop_capture=, pdaop_verbose=, pdaop_delay=, pdaop_diag= are this driver's)
#   MIC_CPUS      guest processors (default 8)
#   MIC_SECONDS   length of each capture of the guest script (default 5)
#   MIC_FILES     further files for the RAM root: "source=/destination;..."
#   MIC_TIMEWAIT  PCIE_TIMEWAIT of tools/pcie_iterate.sh (default 1,5)
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$HERE/../../../.." && pwd); cd "$ROOT"
export PATH=$HOME/.nix-profile/bin:$PATH BENCH_WHO=${BENCH_WHO:-sound}
N=${1:?name}
case "$N" in sd4[0-9]|sd5[0-9]) ;; *) echo "boot names are sd40 .. sd59" >&2; exit 2;; esac
[ ! -e "runs/m1-desktop-010/pcie-enum-attempt-$N" ] || { echo "boot name $N was used already" >&2; exit 2; }
[ ! -e runs/bench.finisher.failed ] || { echo "runs/bench.finisher.failed exists: the M1 did not return to Linux; stop" >&2; exit 3; }
NIX="nix-build --extra-experimental-features nix-command --extra-experimental-features flakes"
KC=${MIC_KC:-$(cd "$HERE" && $NIX build-kc.nix --no-out-link 2> "${TMPDIR:-/tmp}/mic-kc-build.$$.log" | tail -1)}
[ -f "$KC/kernel" ] || { tail -30 "${TMPDIR:-/tmp}/mic-kc-build.$$.log" >&2; echo "no collection" >&2; exit 2; }
rm -f "${TMPDIR:-/tmp}/mic-kc-build.$$.log"
STAT=$(cd "$HERE" && $NIX build-pdmic.nix --no-out-link 2>/dev/null | tail -1)
[ -x "$STAT/usr/bin/pdmic-stat" ] || { echo "no pdmic-stat" >&2; exit 2; }
RUNTIME=${MIC_RUNTIME:-240}
[ "$RUNTIME" -le 600 ] || { echo "boots are capped at 600 s" >&2; exit 2; }
W=$(mktemp -d)
sed -e "s/@SECONDS@/${MIC_SECONDS:-5}/g" "${MIC_GUEST:-$HERE/guest/darwin-mic-test.in}" > "$W/guest"
FILES="$STAT/usr/bin/pdmic-stat=/usr/bin/pdmic-stat${MIC_FILES:+;$MIC_FILES}"
PCIE_KC="$KC" PCIE_TIMEWAIT="${MIC_TIMEWAIT:-1,5}" PCIE_BOOTARGS="cpus=${MIC_CPUS:-8} ${MIC_BOOTARGS-pdsound=0 pdaop_diag=1}" \
  PCIE_GUEST_TEMPLATE="$W/guest" PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 PCIE_EXTRA_FILES="$FILES" LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "$RUNTIME" 2>&1 |
  grep -a "\"status\"\|rror\|delivered\|GUEST_CPUS\|No space\|BENCH_TIMES" | cut -c1-240 || true
rm -rf "$W"
grep -o '"status": *"[^"]*"' "runs/m1-desktop-010/pcie-enum-attempt-$N/result.json" 2>/dev/null || echo "no result.json"
"$HERE/tools/mic_summary.sh" "runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin"
