#!/bin/bash
# SPDX-License-Identifier: MIT
# One boot of the M1 with the PDAppleTouchBar collection and a guest script, through
# tools/pcie_iterate.sh, which queues for the bench itself: do not wrap it. The collection is
# handed over with PCIE_KC; nothing shared is written. The digitizer's firmware comes from
# private/firmware/apple (this machine's own file, never committed) and goes into the RAM
# root as /Library/Firmware/apple/dfrmtfw-j293.bin, as the Wi-Fi firmware does.
# Usage: tb_boot.sh <name, prefix tb>
#   TB_KC        collection store path (default: build ../build-kc.nix)
#   TB_GUEST     guest script template (default guest/darwin-touchbar-test.in)
#   TB_RUNTIME   upper bound of the guest's run in seconds, 120 to 600 (default 240)
#   TB_BOOTARGS  further kernel boot arguments (pdtouchbar=, pdtouchbar_bright=, ...)
#   TB_CPUS      guest processors (default 8)
#   TB_NOFW=1    leave the firmware out (the display-only path)
#   TB_FILES     further files for the RAM root: "source=/destination;..."
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
export PATH=$HOME/.nix-profile/bin:$PATH BENCH_WHO=${BENCH_WHO:-touchbar}
N=${1:?name}
case "$N" in tb*) ;; *) echo "boot names are prefixed tb" >&2; exit 2;; esac
NIX="nix-build --extra-experimental-features nix-command --extra-experimental-features flakes --no-out-link"
KC=${TB_KC:-$(cd "$HERE" && $NIX build-kc.nix 2> "${TMPDIR:-/tmp}/tb-kc-build.$$.log" | tail -1)}
[ -f "$KC/kernel" ] || { tail -30 "${TMPDIR:-/tmp}/tb-kc-build.$$.log" >&2; echo "no collection" >&2; exit 2; }
rm -f "${TMPDIR:-/tmp}/tb-kc-build.$$.log"
python3 "$ROOT/integrations/puredarwin/m1-rust-kext/tools/kc_overlap.py" "$KC/kernel" | tail -1
TEST=$(cd "$HERE" && $NIX build-pdtbtest.nix 2>/dev/null | tail -1)
[ -x "$TEST/usr/bin/pdtbtest" ] || { echo "pdtbtest build failed" >&2; exit 2; }
RUNTIME=${TB_RUNTIME:-240}
[ "$RUNTIME" -ge 120 ] && [ "$RUNTIME" -le 600 ] || { echo "boots run 120 to 600 s" >&2; exit 2; }
W=$(mktemp -d)
# the pictures' checksums from the same renderer, built for this machine
"$HERE/host-test/run-host-tests.sh" "$W" > "$W/host.log" || { cat "$W/host.log" >&2; echo "host tests failed" >&2; exit 2; }
crc() { sed -n "s/^TBHOST picture $1 crc32=//p" "$W/host.log"; }
sed -e "s/@CRC_FN@/$(crc layer-fn)/g" -e "s/@CRC_MEDIA@/$(crc layer-media)/g" -e "s/@CRC_F10_HELD@/$(crc layer-fn-f10-held)/g" \
    "${TB_GUEST:-$HERE/guest/darwin-touchbar-test.in}" > "$W/guest"
FILES="$TEST/usr/bin/pdtbtest=/usr/bin/pdtbtest${TB_FILES:+;$TB_FILES}"
FW=$ROOT/private/firmware/apple/dfrmtfw-j293.bin
if [ "${TB_NOFW:-0}" != 1 ]; then
  [ -f "$FW" ] || { echo "no digitizer firmware at $FW (copy this machine's /lib/firmware/apple/dfrmtfw-j293.bin there)" >&2; exit 2; }
  FILES="$FILES;$FW=/Library/Firmware/apple/dfrmtfw-j293.bin"
fi
PCIE_KC="$KC" PCIE_TIMEWAIT="${TB_TIMEWAIT:-1,5}" PCIE_BOOTARGS="cpus=${TB_CPUS:-8} ${TB_BOOTARGS:-}" \
  PCIE_GUEST_TEMPLATE="$W/guest" PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 PCIE_EXTRA_FILES="$FILES" LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "$RUNTIME" 2>&1 |
  grep -a "\"status\"\|rror\|delivered\|GUEST_CPUS\|No space\|BENCH_TIMES" | cut -c1-240 || true
rm -rf "$W"
"$HERE/tools/tb_summary.sh" "runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin"
