#!/bin/bash
# Boot a display-driver (PDDcp) collection once. Usage: tools/dcp_boot.sh <new-name> <kc-store-path> [extra boot-args]
#   DCP_TEST=1 tools/dcp_boot.sh ...   also stages and runs dcp-flip-test in the guest, with pddcp=2
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT"
N=${1:?name}; KC=${2:?kernel collection store path}; EXTRA=${3:-}
ln -sfn "$KC" runs/m1-desktop-010/apcie-host-kc-build-001/result
# DCP_TEST=1: build the KMS flip test (integrations/puredarwin/m1-dcp/dcp-flip-test.nix), stage it
# with its libdrm and the guest script that runs it (DL43 lines), and boot with pddcp=2.
if [ "${DCP_TEST:-0}" = 1 ]; then
  export PATH=$HOME/.nix-profile/bin:$PATH
  T=$(cd integrations/puredarwin/m1-dcp && nix-build --extra-experimental-features nix-command --extra-experimental-features flakes dcp-flip-test.nix --no-out-link)
  export PCIE_EXTRA_FILES="$T/usr/local/bin/dcp-flip-test=/usr/local/bin/dcp-flip-test;$T/usr/local/lib/mesa-asahi/libdrm.2.dylib=/usr/local/lib/mesa-asahi/libdrm.2.dylib"
  # The guest script replaces the supervisor directly (PCIE_GUEST_TEMPLATE is only honoured on the Wi-Fi join path).
  export PCIE_EXTRA_FILES="$PCIE_EXTRA_FILES;${DCP_TEMPLATE:-$ROOT/integrations/puredarwin/m1-dcp/supervisor/darwin-dcp-flip-test.in}=/usr/bin/darwin-desktop-test"
  DCP_MODE=${DCP_MODE:-2}
fi
PCIE_BOOTARGS="pdgpudt=2 pdgpudt_stringprops=1 pddispdt=2 pddcp=${DCP_MODE:-1} $EXTRA" PCIE_SKIP_BUILD=1 LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "${DCP_RUNTIME:-150}" 2>&1 | grep -a '"status"\|rror\|No space' | cut -c1-200
strings -n 6 runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin | grep -a 'PDDcp\|pd platform: no\|panic\|PD_STUB\|Backtrace\|IOMemoryDescriptor phys\|DL4[0-9]' | grep -av 'PDDcp:   reg \|interrupt controller none\|dart-regs\|dart-l1' | cut -c1-${DCP_COLS:-260}
