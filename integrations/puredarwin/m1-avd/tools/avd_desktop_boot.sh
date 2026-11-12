#!/bin/bash
# SPDX-License-Identifier: MIT
# One unattended boot of the GPU desktop (as tools/gpu_desktop.sh stages it: compositor on GL,
# sound, fast boot) with the PDAppleAVD collection, the decoder's firmware, pdvideo built
# against the FFmpeg with the pdavd hwaccel, and clips; once the compositor is up, each line
# of the play list is run as a pdvideo command and its PDVIDEO_STATS line goes to the console.
# Speakers held 20 dB down (hold_probe=1) and a low volume: nobody is listening.
# Queues for the bench itself. Usage: avd_desktop_boot.sh <name, prefix av> <play list file>
#
# For a person at the machine:  AVD_INTERACTIVE=1 avd_desktop_boot.sh <name> [minutes, default 30]
#   the desktop of tools/gpu_desktop.sh for that long with the player and the clips staged,
#   speakers not held down, no play list. In a terminal there:
#     pdvideo /private/var/avd/bbb-1080p-h264-60s.mp4          (q or Esc ends it)
#     pdvideo --hwdec=none ...    software decode, for comparison;   --fullscreen;   --volume=0..100
#   End early from the workstation: tools/gpu_desktop_stop.sh
#   AVD_KC       collection (default: build ../build-kc.nix)
#   AVD_SECONDS  desktop lifetime (default 120); the boot's upper bound is that + 100
#   AVD_CLIPS    clips staged under /private/var/avd ("a.mp4 b.webm", default: the 1080p 10 s three)
#   AVD_VOLUME   headphone attenuation at start (default 45)
# A play list line starts with the program: "pdvideo --stats ..." or "pdvideo-gl --vo=gl ...".
set -euo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
export PATH=$HOME/.nix-profile/bin:$PATH BENCH_WHO=${BENCH_WHO:-avd}
# What this script stages from Nix besides the collection (tools, libraries, the audio server) are still
# the builds of the pinned revision, which run on the tip's kernel and libSystem (SF-V-rebase.md).
export SF_PD=pin
N=${1:?name}
case "$N" in av*) ;; *) echo "boot names are prefixed av" >&2; exit 2;; esac
if [ "${AVD_INTERACTIVE:-0}" = 1 ]; then
  MIN=${2:-30}; [ "$MIN" -ge 2 ] && [ "$MIN" -le 235 ] || { echo "minutes must be 2..235" >&2; exit 2; }
  DESK=$((MIN * 60)); LIMIT=$((DESK + 20)); LIST=/dev/null; HOLD=0; VOLUME=${AVD_VOLUME:-30}
  : "${AVD_CLIPS:=bbb-1080p-h264-60s.mp4}"       # what fits: the RAM root has about 15 MB for player and clips
else
  LIST=${2:?play list}
  DESK=${AVD_SECONDS:-120}; LIMIT=$((DESK + 100)); HOLD=1; VOLUME=${AVD_VOLUME:-45}
  [ "$LIMIT" -ge 120 ] && [ "$LIMIT" -le 600 ] || { echo "the upper bound is 120..600 seconds" >&2; exit 2; }
fi
NP=integrations/puredarwin/m1-boot-speed/tools/bs_nix_path.py
KC=${AVD_KC:-$("$ROOT/tools/default_kc.sh")}
[ -f "$KC/kernel" ] || { echo "no collection" >&2; exit 2; }
T=$($NP m1-mesa-asahi agx-offscreen-test.nix)
D=runs/m1-desktop-010/wm-darwin-scale-desktop-build-009/target/aarch64-apple-darwin/release/chonkstep-darwin-desktop
TERM_BIN=runs/m1-desktop-010/terminal-application-build-008/target/aarch64-apple-darwin/release/chonk-terminal-darwin
[ -x "$D" ] && [ -x "$TERM_BIN" ] || { echo "no compositor or terminal binary" >&2; exit 2; }
X="$ROOT/$D=/usr/bin/chonkstep-darwin-desktop;$ROOT/$TERM_BIN=/usr/bin/chonk-terminal-darwin"
TI=integrations/puredarwin/chonkstep-darwin/terminfo/xterm-256color
[ -r "$TI" ] && X="$X;$ROOT/$TI=/usr/share/terminfo/x/xterm-256color;$ROOT/$TI=/usr/share/terminfo/78/xterm-256color"
BG=integrations/puredarwin/chonkstep-darwin/backgrounds/siliconforest.png
X="$X;$ROOT/$BG=/usr/share/backgrounds/siliconforest/siliconforest.png"
# With the GL player in the list the RAM root cannot hold two Mesa bundles (32 MB each): the
# compositor is then pointed at the player's bundle (the same Mesa with the Wayland platform
# added) through CHONKSTEP_DARWIN_GL_LIBDIR, for that boot only.
GLENV=
if [ "${AVD_INTERACTIVE:-0}" != 1 ] && grep -q '^pdvideo-gl' "$LIST"; then
  GLENV='export CHONKSTEP_DARWIN_GL_LIBDIR=/usr/local/lib/mesa-asahi-wl'
else
  for l in libEGL.1.dylib libGLESv2.2.dylib libgallium-26.3.0-devel.dylib libdrm.2.dylib; do X="$X;$T/usr/local/lib/mesa-asahi/$l=/usr/local/lib/mesa-asahi/$l"; done
fi
A=$($NP m1-sound/sysaudio build-sysaudio.nix -A all)
[ -x "$A/usr/sbin/coreaudiod" ] || { echo "sysaudio build failed" >&2; exit 2; }
FW=System/Library/Frameworks
X="$X;$A/usr/sbin/coreaudiod=/usr/sbin/coreaudiod;$A/usr/bin/pdtone=/usr/bin/pdtone;$A/usr/bin/pdvolume=/usr/bin/pdvolume"
for f in CoreAudio AudioToolbox AudioUnit; do X="$X;$A/$FW/$f.framework/Versions/A/$f=/$FW/$f.framework/Versions/A/$f"; done
TBFW=private/firmware/apple/dfrmtfw-j293.bin
[ -f "$TBFW" ] && X="$X;$ROOT/$TBFW=/Library/Firmware/apple/dfrmtfw-j293.bin"
AVFW=private/firmware/apple/avd-fw-v2-t0.bin
[ -f "$AVFW" ] || { echo "no decoder firmware at $AVFW" >&2; exit 2; }
X="$X;$ROOT/$AVFW=/Library/Firmware/apple/avd-fw-v2-t0.bin"
# the player with hardware decode, and its GL variant with the Wayland Mesa bundle it links
P=$($NP m1-avd build-pdvideo-pdavd.nix)
[ -x "$P/usr/bin/pdvideo" ] || { echo "pdvideo build failed" >&2; exit 2; }
# (left out when the play list only runs the GL variant: the RAM root does not hold both with Mesa's second bundle)
if [ "${AVD_INTERACTIVE:-0}" = 1 ] || grep -q '^pdvideo ' "$LIST"; then X="$X;$P/usr/bin/pdvideo=/usr/local/bin/pdvideo"; fi
if [ "${AVD_INTERACTIVE:-0}" != 1 ] && grep -q '^pdvideo-gl' "$LIST"; then
  G=$($NP m1-avd build-pdvideo-pdavd.nix --arg gl true)
  [ -x "$G/usr/bin/pdvideo-gl" ] || { echo "pdvideo-gl build failed" >&2; exit 2; }
  X="$X;$G/usr/bin/pdvideo-gl=/usr/local/bin/pdvideo-gl"
  for l in "$G"/usr/local/lib/mesa-asahi-wl/*.dylib; do X="$X;$l=/usr/local/lib/mesa-asahi-wl/$(basename "$l")"; done
fi
BYTES=0
for c in ${AVD_CLIPS:-bbb-1080p-h264-10s.mp4 bbb-1080p-hevc-10s.mp4 bbb-1080p-vp9-10s.webm}; do
  f=runs/avd/clips/$c; [ -f "$f" ] || f=runs/avd/t/$c
  [ -f "$f" ] || { echo "no clip $c" >&2; exit 2; }
  X="$X;$ROOT/$f=/private/var/avd/$c"; BYTES=$((BYTES + $(stat -c %s "$f")))
done
[ "$BYTES" -le 40000000 ] || { echo "clips are $BYTES bytes; keep them under 40 MB" >&2; exit 2; }
W=$(mktemp -d)
cp "$LIST" "$W/play.list"; X="$X;$W/play.list=/private/var/avd/play.list"
python3 - integrations/puredarwin/chonkstep-darwin/darwin-gpu-desktop-interactive.in "$HERE/guest/avd-desktop-play.in" "$W/guest" "$DESK" "$VOLUME" "$HOLD" "$GLENV" <<'PY'
import sys
base, play, out, seconds, volume, hold, glenv = sys.argv[1:]
text = open(base).read()
mark = 'records=$runtime/session-$session\n'
assert text.count(mark) == 1
text = text.replace(mark, glenv + '\n' + open(play).read() + mark)
text = text.replace('@SECONDS@', seconds).replace('@HOLD@', hold).replace('@VOLUME@', volume)
open(out, 'w').write(text)
PY
Q=integrations/puredarwin/m1-quiet-boot; B=integrations/puredarwin/m1-bringup
python3 "$Q/boot_logo.py" pack "$BG" "$W" > /dev/null
env -u PCIE_CHAIN BS_RAMDISK_MIB=${BS_RAMDISK_MIB:-464} PCIE_LAB_PATCH="$ROOT/integrations/puredarwin/m1-boot-speed/launcher-fast-quiet.patch" \
  PCIE_LAB_FILES="$W/boot-logo.gz=$B/boot-logo.gz;$W/boot-logo.json=$B/boot-logo.json" \
  PCIE_STAGER="$ROOT/integrations/puredarwin/m1-boot-speed/tools/stage_ramdisk_slim.py" PCIE_KC="$KC" \
  PCIE_BOOTARGS="cpus=8 pdgpudt=2 pdgpudt_stringprops=1 pdagx=1 pddispdt=2 pddcp=2 pd_quiet_wait=0 pd_rtc_wait=0 pdavd=${AVD_LEVEL:-1} ${AVD_BOOTARGS:-}" \
  PCIE_EXTRA_FILES="$X" PCIE_GUEST_TEMPLATE="$W/guest" PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "$LIMIT" > "runs/avd/$N.iterate.log" 2>&1 || true
grep -a '"status"\|rror\|delivered\|No space\|ValueError' "runs/avd/$N.iterate.log" | cut -c1-240 || true
rm -rf "$W"
C=runs/m1-desktop-010/pcie-enum-attempt-$N/guest/console.bin
strings -n 4 "$C" | grep -a 'AVDPLAY\|PDAppleAVD:\|PDAvdDart:\|GPUDESK_DESKTOP\|GPUDESK_COREAUDIOD\|GPUDESK_SOUND_END\|panic' | cut -c1-900
