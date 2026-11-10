#!/bin/bash
# Boot a display-driver (PDDcp) collection once. Usage: tools/dcp_boot.sh <new-name> <kc-store-path> [extra boot-args]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT"
N=${1:?name}; KC=${2:?kernel collection store path}; EXTRA=${3:-}
ln -sfn "$KC" runs/m1-desktop-010/apcie-host-kc-build-001/result
PCIE_BOOTARGS="pdgpudt=2 pdgpudt_stringprops=1 pddispdt=2 pddcp=${DCP_MODE:-1} $EXTRA" PCIE_SKIP_BUILD=1 LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "${DCP_RUNTIME:-150}" 2>&1 | grep -a '"status"\|rror\|No space' | cut -c1-200
strings -n 6 runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin | grep -a 'PDDcp\|pd platform: no\|panic\|PD_STUB\|Backtrace\|IOMemoryDescriptor phys\|DL4[0-9]' | grep -av 'PDDcp:   reg \|interrupt controller none\|dart-regs\|dart-l1' | cut -c1-${DCP_COLS:-260}
