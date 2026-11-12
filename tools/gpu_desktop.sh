#!/bin/bash
# Boot the GPU desktop on the M1 for a person to use: Wi-Fi + GPU + display drivers, the
# compositor on GL with page flips, real keyboard and trackpad, two terminals.
# Usage: tools/gpu_desktop.sh <new-name> [minutes, default 30, max 235]
# End early from this machine: tools/gpu_desktop_stop.sh   (the M1 returns to Linux in about a minute)
# Quiet boot (default): the desk picture is put on the panel before the guest starts and the
# kernel draws nothing on it (graphics boot, console on the UART only), so the picture stays
# until the compositor's first frame. The serial log is the same as before.
#   GPU_DESKTOP_VERBOSE=1       the text boot: kernel log on the panel as well as on serial
#   GPU_DESKTOP_SPLASH=<png>    another boot picture (default: the desk picture)
# Sound and the Touch Bar are on: coreaudiod runs, F10-F12 on the strip are mute and volume.
#   GPU_DESKTOP_SPEAKER_HOLD=1  keep the speakers 20 dB under the protection model's limit
#   GPU_DESKTOP_VOLUME=<n>      headphone attenuation at start (default 30)
# The session also ends by itself after the given time, and the M1 is returned to Linux.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT"; export PATH=$HOME/.nix-profile/bin:$PATH
N=${1:?name}; MIN=${2:-30}
[ "$MIN" -ge 2 ] && [ "$MIN" -le 235 ] || { echo "minutes must be 2..235" >&2; exit 2; }
BOOT=140                                   # seconds from hypervisor start to the guest script
RUNTIME=$((MIN * 60 + BOOT)); DESK=$((MIN * 60))
# The default collection: the desktop collection with sound and the Touch Bar
# (m1-touchbar/build-kc.nix: the quiet-boot kernel, PDAppleSound, PDAppleTouchBar; boots tb05, tb06).
KC=${GPU_DESKTOP_KC:-/nix/store/wadn9g1r46g28gdhh0c4k3qcgnxxpvkr-puredarwin-kc-arm64-0.1}
[ -f "$KC/kernel" ] || KC=$(cd integrations/puredarwin/m1-touchbar && nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kc.nix --no-out-link | tail -1)
NB="nix-build --extra-experimental-features nix-command --extra-experimental-features flakes --no-out-link"
T=$(cd integrations/puredarwin/m1-mesa-asahi && $NB agx-offscreen-test.nix 2>/dev/null | tail -1)
# The compositor with display scaling, the settings menu, volume keys and the Touch Bar phase (wm-darwin merge, boot ds06).
D=${GPU_DESKTOP_BIN:-runs/m1-desktop-010/wm-darwin-scale-desktop-build-009/target/aarch64-apple-darwin/release/chonkstep-darwin-desktop}
[ -x "$D" ] || { echo "no compositor binary at $D" >&2; exit 2; }
X="$ROOT/$D=/usr/bin/chonkstep-darwin-desktop"
# The terminal that accepts lifetimes above 120 s (build-003: --seconds up to 14400, 8 MiB of
# output and 1 MiB of input per terminal). The desktop passes each terminal the session's
# remaining time, which the RAM root's older terminal refuses.
# build-004 added the xterm emulation (full-screen programs, scrollback, TERM=xterm-256color),
# build-005 selection by dragging and copy/paste.
TERM_BIN=runs/m1-desktop-010/terminal-application-build-008/target/aarch64-apple-darwin/release/chonk-terminal-darwin
[ -x "$TERM_BIN" ] || { echo "no terminal binary at $TERM_BIN" >&2; exit 2; }
X="$X;$ROOT/$TERM_BIN=/usr/bin/chonk-terminal-darwin"
# The terminfo entry for that TERM; the RAM root has none.
TI=integrations/puredarwin/chonkstep-darwin/terminfo/xterm-256color
[ -r "$TI" ] && X="$X;$ROOT/$TI=/usr/share/terminfo/x/xterm-256color;$ROOT/$TI=/usr/share/terminfo/78/xterm-256color"
# The desk picture; the guest script points CHONKSTEP_BACKGROUND_DIR at its directory.
BG=${GPU_DESKTOP_BACKGROUND:-integrations/puredarwin/chonkstep-darwin/backgrounds/siliconforest.png}
[ -f "$BG" ] && X="$X;$ROOT/$BG=/usr/share/backgrounds/siliconforest/siliconforest.png"
for l in libEGL.1.dylib libGLESv2.2.dylib libgallium-26.3.0-devel.dylib libdrm.2.dylib; do X="$X;$T/usr/local/lib/mesa-asahi/$l=/usr/local/lib/mesa-asahi/$l"; done
# Sound: the audio server, its frameworks and the tools (as m1-sound/tools/sd_desktop_boot.sh).
SOUND=integrations/puredarwin/m1-sound
A=$(cd $SOUND/sysaudio && $NB build-sysaudio.nix -A all 2>/dev/null | tail -1)
[ -x "$A/usr/sbin/coreaudiod" ] || { echo "sysaudio build failed" >&2; exit 2; }
PLAY=$(cd $SOUND && $NB build-pdplay.nix 2>/dev/null | tail -1)
FW=System/Library/Frameworks
X="$X;$A/usr/sbin/coreaudiod=/usr/sbin/coreaudiod;$A/usr/bin/pdtone=/usr/bin/pdtone;$A/usr/bin/pdvolume=/usr/bin/pdvolume;$PLAY/usr/bin/pdplay=/usr/bin/pdplay"
for f in CoreAudio AudioToolbox AudioUnit; do X="$X;$A/$FW/$f.framework/Versions/A/$f=/$FW/$f.framework/Versions/A/$f"; done
# The Touch Bar digitizer's firmware (this machine's own, never committed); without it the
# strip shows its keys but takes no touches.
TBFW=private/firmware/apple/dfrmtfw-j293.bin
[ -f "$TBFW" ] && X="$X;$ROOT/$TBFW=/Library/Firmware/apple/dfrmtfw-j293.bin" || echo "no Touch Bar firmware at $TBFW: the strip will not take touches" >&2
# Speakers are unrestricted: the kernel protection model alone sets the limit (owner decision,
# 2026-11-11; no person has listened yet). GPU_DESKTOP_SPEAKER_HOLD=1 keeps them 20 dB under it.
HOLD=${GPU_DESKTOP_SPEAKER_HOLD:-0}; VOL=${GPU_DESKTOP_VOLUME:-30}
QUIET=()
if [ "${GPU_DESKTOP_VERBOSE:-0}" != 1 ]; then
  # The launcher patch (graphics boot, no mirror, paints boot-logo.gz) and the picture in the
  # framebuffer's format, both for this boot's private copy of the launcher tree only.
  Q=integrations/puredarwin/m1-quiet-boot; SPLASH=${GPU_DESKTOP_SPLASH:-$BG}
  [ -f "$SPLASH" ] || { echo "no boot picture at $SPLASH" >&2; exit 2; }
  QD=$(mktemp -d); python3 "$Q/boot_logo.py" pack "$SPLASH" "$QD" > /dev/null
  B=integrations/puredarwin/m1-bringup
  QUIET=(PCIE_LAB_PATCH="$ROOT/$Q/quiet-boot.patch" PCIE_LAB_FILES="$QD/boot-logo.gz=$B/boot-logo.gz;$QD/boot-logo.json=$B/boot-logo.json")
fi
S=$(mktemp); sed -e "s/@SECONDS@/$DESK/g" -e "s/@HOLD@/$HOLD/g" -e "s/@VOLUME@/$VOL/g" integrations/puredarwin/chonkstep-darwin/darwin-gpu-desktop-interactive.in > "$S"
echo "GPU desktop: $MIN minutes; the desktop appears about 2.5 minutes after the M1 reboots"
env "${QUIET[@]}" PCIE_KC="$KC" PCIE_BOOTARGS="cpus=${GPU_DESKTOP_CPUS:-8} pdgpudt=2 pdgpudt_stringprops=1 pdagx=1 pddispdt=2 pddcp=2" PCIE_EXTRA_FILES="$X" PCIE_GUEST_TEMPLATE="$S" \
PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 LINES_MAX=1 tools/pcie_iterate.sh "$N" "$RUNTIME" 2>&1 | grep -a '"status"\|rror\|delivered' | cut -c1-240
rm -rf "$S" ${QD:+"$QD"}
strings -n 6 "runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin" | grep -a 'GPUDESK\|WMSTAT\|panic\|PDSpeakers:\|PDTouchBar:' | cut -c1-260 | tail -40
