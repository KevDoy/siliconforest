#!/bin/bash
# SPDX-License-Identifier: MIT
# One unattended whole-desktop boot on the M1 with a libSystem under test staged as
# /usr/lib/libSystem.B.dylib for this boot only (no default is changed): the proof that a
# candidate default libSystem (../build-libsystem.nix, GitHub issue 46) carries the desktop.
# It is ../../m1-desktop-scale/tools/ds_desktop_boot.sh (same guest script, compositor
# build-009, terminal build-008, all autopilot phases, Wi-Fi join, sound, Touch Bar) with
# the fast-boot settings of tools/gpu_desktop.sh (the default collection of m1-boot-speed,
# pd_quiet_wait=0 pd_rtc_wait=0, launcher-fast-quiet.patch, the slim stager).
# Queues for the bench itself (BENCH_WHO=libsystem). PCIE_CHAIN stays off.
# Usage: ls_desktop_boot.sh <name, prefix ls> [upper bound in seconds, 120..600, default 480]
#   LS_LIBSYSTEM   store path of the libSystem build to stage (default: ../build-libsystem.nix)
#   LS_LIBSYSTEM=default   stage nothing: the RAM root's own libSystem (a control boot)
#   DS_* as in ds_desktop_boot.sh
set -euo pipefail
LSHERE=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$LSHERE/../../.." && pwd); cd "$ROOT"
HERE=$ROOT/integrations/puredarwin/m1-desktop-scale
export PATH=$HOME/.nix-profile/bin:$PATH BENCH_WHO=${BENCH_WHO:-libsystem}
# What this script stages from Nix besides the collection (tools, libraries, the audio server) are still
# the builds of the pinned revision, which run on the tip's kernel and libSystem (SF-V-rebase.md).
export SF_PD=pin
N=${1:?name}; LIMIT=${2:-480}
case "$N" in ls*) ;; *) echo "boot names are prefixed ls" >&2; exit 2;; esac
[ "$LIMIT" -ge 120 ] && [ "$LIMIT" -le 600 ] || { echo "the upper bound is 120..600 seconds" >&2; exit 2; }
SECONDS_DESK=${DS_SECONDS:-215}; AUTO=${DS_AUTOPILOT:-1}
[ "$SECONDS_DESK" -ge 11 ] && [ "$SECONDS_DESK" -le 14400 ] || { echo "DS_SECONDS is 11..14400" >&2; exit 2; }
case "${DS_SCALE:-}" in ''|1|1.25|1.5|1.75|2|2.5|3) ;; *) echo "DS_SCALE is one of 1 1.25 1.5 1.75 2 2.5 3" >&2; exit 2;; esac
case "${DS_THEME:-}${DS_ONLY:-}" in *[!a-z0-9,-]*) echo "DS_THEME and DS_ONLY are lower-case ids" >&2; exit 2;; esac
KC=${DS_KC:-$("$ROOT/tools/default_kc.sh")}
[ -f "$KC/kernel" ] || { echo "no collection at $KC" >&2; exit 2; }
NB="nix-build --extra-experimental-features nix-command --extra-experimental-features flakes --no-out-link"
T=$(cd integrations/puredarwin/m1-mesa-asahi && $NB agx-offscreen-test.nix 2>/dev/null | tail -1)
D=${DS_DESKTOP_BIN:-$ROOT/runs/m1-desktop-010/wm-darwin-scale-desktop-build-009/target/aarch64-apple-darwin/release/chonkstep-darwin-desktop}
[ -x "$D" ] || { echo "no compositor binary at $D" >&2; exit 2; }
X="$D=/usr/bin/chonkstep-darwin-desktop"
# The libSystem under test, for this boot's RAM root only (tools/pcie_assemble.py replaces the
# base mapping's entry for a destination named in PCIE_EXTRA_FILES).
LS=${LS_LIBSYSTEM:-}
if [ -z "$LS" ]; then LS=$(cd "$LSHERE" && $NB build-libsystem.nix 2>/dev/null | tail -1); fi
if [ "$LS" != default ]; then
  [ -f "$LS/usr/lib/libSystem.B.dylib" ] || { echo "no libSystem at $LS" >&2; exit 2; }
  X="$X;$LS/usr/lib/libSystem.B.dylib=/usr/lib/libSystem.B.dylib"
  echo "libSystem staged: $LS ($(sha256sum "$LS/usr/lib/libSystem.B.dylib" | cut -c1-16))"
fi
TERM_BIN=${DS_TERMINAL_BIN:-$ROOT/runs/m1-desktop-010/terminal-application-build-008/target/aarch64-apple-darwin/release/chonk-terminal-darwin}
[ -x "$TERM_BIN" ] || { echo "no terminal binary at $TERM_BIN" >&2; exit 2; }
X="$X;$TERM_BIN=/usr/bin/chonk-terminal-darwin"
TI=$ROOT/integrations/puredarwin/chonkstep-darwin/terminfo/xterm-256color
[ -r "$TI" ] && X="$X;$TI=/usr/share/terminfo/x/xterm-256color;$TI=/usr/share/terminfo/78/xterm-256color"
BG=$ROOT/integrations/puredarwin/chonkstep-darwin/backgrounds/siliconforest.png
[ -f "$BG" ] && X="$X;$BG=/usr/share/backgrounds/siliconforest/siliconforest.png"
for l in libEGL.1.dylib libGLESv2.2.dylib libgallium-26.3.0-devel.dylib libdrm.2.dylib; do X="$X;$T/usr/local/lib/mesa-asahi/$l=/usr/local/lib/mesa-asahi/$l"; done
# Sound: the server, its frameworks and the tools (as tools/gpu_desktop.sh).
SOUND=$ROOT/integrations/puredarwin/m1-sound
A=$(cd "$SOUND/sysaudio" && $NB build-sysaudio.nix -A all 2>/dev/null | tail -1)
[ -x "$A/usr/sbin/coreaudiod" ] || { echo "sysaudio build failed" >&2; exit 2; }
FW=System/Library/Frameworks
X="$X;$A/usr/sbin/coreaudiod=/usr/sbin/coreaudiod;$A/usr/bin/pdtone=/usr/bin/pdtone;$A/usr/bin/pdvolume=/usr/bin/pdvolume"
for f in CoreAudio AudioToolbox AudioUnit; do X="$X;$A/$FW/$f.framework/Versions/A/$f=/$FW/$f.framework/Versions/A/$f"; done
# The Touch Bar digitizer's firmware (this machine's own, never committed), when it is here.
TBFW=$ROOT/private/firmware/apple/dfrmtfw-j293.bin
[ -f "$TBFW" ] && X="$X;$TBFW=/Library/Firmware/apple/dfrmtfw-j293.bin"
QUIET=()
QD=
FASTARGS=
if [ "${DS_VERBOSE:-0}" != 1 ] && [ -f "$BG" ]; then
  Q=integrations/puredarwin/m1-quiet-boot
  QD=$(mktemp -d); python3 "$Q/boot_logo.py" pack "$BG" "$QD" > /dev/null
  B=integrations/puredarwin/m1-bringup
  # fast boot: the launcher patch that types the challenge once the console is silent
  QUIET=(PCIE_LAB_PATCH="$ROOT/integrations/puredarwin/m1-boot-speed/launcher-fast-quiet.patch" PCIE_LAB_FILES="$QD/boot-logo.gz=$B/boot-logo.gz;$QD/boot-logo.json=$B/boot-logo.json")
  FASTARGS="pd_quiet_wait=0 pd_rtc_wait=0"
fi
S=$(mktemp)
# Unattended: frame statistics every 120 frames and the speakers held; by hand: as tools/gpu_desktop.sh.
if [ "$AUTO" = 1 ]; then STATS=120; HOLD=1; else STATS=3600; HOLD=${DS_SPEAKER_HOLD:-0}; fi
sed -e "s/@SECONDS@/$SECONDS_DESK/g" -e "s/@AUTOPILOT@/$AUTO/g" -e "s/@SCALE@/${DS_SCALE:-}/g" -e "s/@THEME@/${DS_THEME:-}/g" \
    -e "s/@ONLY@/${DS_ONLY:-}/g" -e "s/@SECOND@/${DS_SECOND:-$AUTO}/g" -e "s/@WIFI_TOGGLE@/${DS_WIFI_TOGGLE:-1}/g" \
    -e "s/@RENDERER@/${DS_RENDERER:-gl}/g" -e "s/@STATS@/$STATS/g" -e "s/@HOLD@/$HOLD/g" -e "s/@VOLUME@/${DS_VOLUME:-30}/g" \
    "$HERE/guest/darwin-desktop-scale.in" > "$S"
env -u PCIE_CHAIN "${QUIET[@]}" PCIE_STAGER="$LSHERE/tools/ls_stager.py" PCIE_KC="$KC" PCIE_BOOTARGS="cpus=8 pdgpudt=2 pdgpudt_stringprops=1 pdagx=1 pddispdt=2 pddcp=2 $FASTARGS ${DS_BOOTARGS:-}" PCIE_EXTRA_FILES="$X" \
  PCIE_GUEST_TEMPLATE="$S" PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 LINES_MAX=1 tools/pcie_iterate.sh "$N" "$LIMIT" 2>&1 |
  grep -a '"status"\|rror\|delivered\|No space' | cut -c1-240 || true
rm -rf "$S" ${QD:+"$QD"}
"$HERE/tools/ds_summary.sh" "$N"
