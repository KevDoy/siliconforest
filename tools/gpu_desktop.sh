#!/bin/bash
# Boot the GPU desktop on the M1 for a person to use: Wi-Fi + GPU + display drivers, the
# compositor on GL with page flips, real keyboard and trackpad, two terminals.
# Usage: tools/gpu_desktop.sh <new-name> [minutes, default 30, max 235]
# End early from this machine: tools/gpu_desktop_stop.sh   (the M1 returns to Linux in about a minute)
# The session also ends by itself after the given time, and the M1 is returned to Linux.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT"; export PATH=$HOME/.nix-profile/bin:$PATH
N=${1:?name}; MIN=${2:-30}
[ "$MIN" -ge 2 ] && [ "$MIN" -le 235 ] || { echo "minutes must be 2..235" >&2; exit 2; }
BOOT=140                                   # seconds from hypervisor start to the guest script
RUNTIME=$((MIN * 60 + BOOT)); DESK=$((MIN * 60))
KC=${GPU_DESKTOP_KC:-/nix/store/k7xlish9lxldkjvkfccjyj0sg9ddjgz1-puredarwin-kc-arm64-0.1}
[ -f "$KC/kernel" ] || KC=$(cd integrations/puredarwin/m1-smp && nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kc-desktop.nix --no-out-link | tail -1)
ln -sfn "$KC" runs/m1-desktop-010/bwfm-kc-build-001/result
NB="nix-build --extra-experimental-features nix-command --extra-experimental-features flakes --no-out-link"
T=$(cd integrations/puredarwin/m1-mesa-asahi && $NB agx-offscreen-test.nix 2>/dev/null | tail -1)
D=runs/m1-desktop-010/wm-darwin-gl-desktop-build-001/target/aarch64-apple-darwin/release/chonkstep-darwin-desktop
X="$ROOT/$D=/usr/bin/chonkstep-darwin-desktop"
# The terminal that accepts lifetimes above 120 s (build-003: --seconds up to 14400, 8 MiB of
# output and 1 MiB of input per terminal). The desktop passes each terminal the session's
# remaining time, which the RAM root's older terminal refuses.
# build-004 added the xterm emulation (full-screen programs, scrollback, TERM=xterm-256color),
# build-005 selection by dragging and copy/paste.
TERM_BIN=runs/m1-desktop-010/terminal-application-build-005/target/aarch64-apple-darwin/release/chonk-terminal-darwin
[ -x "$TERM_BIN" ] || { echo "no terminal binary at $TERM_BIN" >&2; exit 2; }
X="$X;$ROOT/$TERM_BIN=/usr/bin/chonk-terminal-darwin"
# The terminfo entry for that TERM; the RAM root has none.
TI=integrations/puredarwin/chonkstep-darwin/terminfo/xterm-256color
[ -r "$TI" ] && X="$X;$ROOT/$TI=/usr/share/terminfo/x/xterm-256color;$ROOT/$TI=/usr/share/terminfo/78/xterm-256color"
# The desk picture; the guest script points CHONKSTEP_BACKGROUND_DIR at its directory.
BG=${GPU_DESKTOP_BACKGROUND:-integrations/puredarwin/chonkstep-darwin/backgrounds/siliconforest.png}
[ -f "$BG" ] && X="$X;$ROOT/$BG=/usr/share/backgrounds/siliconforest/siliconforest.png"
for l in libEGL.1.dylib libGLESv2.2.dylib libgallium-26.3.0-devel.dylib libdrm.2.dylib; do X="$X;$T/usr/local/lib/mesa-asahi/$l=/usr/local/lib/mesa-asahi/$l"; done
S=$(mktemp); sed "s/@SECONDS@/$DESK/g" integrations/puredarwin/chonkstep-darwin/darwin-gpu-desktop-interactive.in > "$S"
echo "GPU desktop: $MIN minutes; the desktop appears about 2.5 minutes after the M1 reboots"
PCIE_BOOTARGS="cpus=${GPU_DESKTOP_CPUS:-8} pdgpudt=2 pdgpudt_stringprops=1 pdagx=1 pddispdt=2 pddcp=2" PCIE_EXTRA_FILES="$X" PCIE_GUEST_TEMPLATE="$S" \
PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 LINES_MAX=1 tools/pcie_iterate.sh "$N" "$RUNTIME" 2>&1 | grep -a '"status"\|rror\|delivered' | cut -c1-240
rm -f "$S"
strings -n 6 "runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin" | grep -a 'GPUDESK\|WMSTAT\|panic' | cut -c1-260 | tail -40
