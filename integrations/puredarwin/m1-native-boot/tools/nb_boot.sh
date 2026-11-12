#!/bin/bash
# SPDX-License-Identifier: MIT
# One boot of the native-boot stream (docs/roadmap/issues/SF-T-native-boot.md), RAM-loaded
# over USB like every other boot. Queues for the bench itself.
# Usage: nb_boot.sh <name, prefix nb> [upper bound in seconds, 120..600, default 175]
#   NB_MODE=probe    the hypervisor boot, with the controller listening on the M1's real UART
#   NB_MODE=native   no hypervisor (../native_boot.py); needs NB_RETURN
#   NB_RETURN=vdm    native: the controller resets the M1 with its usual `macvdmtool reboot
#                    serial` when the capture is over (launcher-native.patch, word vdm-return)
#   NB_WDT=<s>       native: also arm the SoC watchdog for <s> seconds (30..170) before the
#                    kernel starts; with NB_RETURN unset it is the only way back, the capture
#                    then always runs the whole upper bound, which must be NB_WDT + 45 or more
#   NB_PROFILE=shell (default) one processor, no GPU or display arguments, ../guest-shell.in
#   NB_PROFILE=desktop  the default desktop as m1-boot-speed/tools/bs_boot.sh boots it
#   NB_CPUS          processors (default 1 for shell, 8 for desktop)
#   NB_SECONDS       desktop session length (default 30)
#   NB_KC            kernel collection (default: the default desktop collection)
#   NB_BOOTARGS      further boot arguments;  NB_WORDS  further words for native_mode.txt
# The whole script is one function, read completely before anything runs.
main() {
set -euo pipefail
HERE=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
export PATH=$HOME/.nix-profile/bin:$PATH BENCH_WHO=${BENCH_WHO:-native}
N=${1:?name}; LIMIT=${2:-175}
case "$N" in nb*) ;; *) echo "boot names are prefixed nb" >&2; exit 2;; esac
[ "$LIMIT" -ge 120 ] && [ "$LIMIT" -le 600 ] || { echo "the upper bound is 120..600 seconds" >&2; exit 2; }
[ ! -e "runs/m1-desktop-010/pcie-enum-attempt-$N" ] || { echo "boot $N exists" >&2; exit 2; }
[ -z "${PCIE_CHAIN:-}" ] || { echo "PCIE_CHAIN is not used in this stream" >&2; exit 2; }
MODE=${NB_MODE:?NB_MODE=probe or native}; PROFILE=${NB_PROFILE:-shell}; DONE=marker
case "$MODE" in
  probe) WORDS=probe;;
  native)
    WORDS=native
    [ "${NB_RETURN:-}" = vdm ] && WORDS="$WORDS vdm-return"
    if [ -n "${NB_WDT:-}" ]; then
      WORDS="$WORDS wdt=$NB_WDT"
      if [ "${NB_RETURN:-}" != vdm ]; then
        [ "$LIMIT" -ge $((NB_WDT + 45)) ] || { echo "without NB_RETURN=vdm the upper bound must be NB_WDT + 45 or more" >&2; exit 2; }
        DONE=off
      fi
    fi
    [ "${NB_RETURN:-}" = vdm ] || [ -n "${NB_WDT:-}" ] || { echo "a native boot needs NB_RETURN=vdm, NB_WDT=<s>, or both" >&2; exit 2; };;
  *) echo "NB_MODE is probe or native" >&2; exit 2;;
esac
WORDS="$WORDS ${NB_WORDS:-}"
KC=${NB_KC:-/nix/store/z66w83kk8gj61xv5ych34rz1zv18hk70-puredarwin-kc-arm64-0.1}
[ -f "$KC/kernel" ] || { echo "no collection at $KC" >&2; exit 2; }
B=integrations/puredarwin/m1-bringup
QD=$(mktemp -d); S=$(mktemp); trap 'rm -rf "$QD" "$S"' EXIT
echo "$WORDS" > "$QD/native_mode.txt"
python3 -B -c "import sys; sys.path.insert(0, '$HERE'); import native_boot; native_boot.Mode(open('$QD/native_mode.txt').read())"
FILES="$HERE/native_boot.py=$B/native_boot.py;$HERE/native_stub.py=$B/native_stub.py;$QD/native_mode.txt=$B/native_mode.txt"
WAITS="pd_quiet_wait=0 pd_rtc_wait=0"; X=
if [ "$PROFILE" = desktop ]; then
  NP=integrations/puredarwin/m1-boot-speed/tools/bs_nix_path.py
  T=$($NP m1-mesa-asahi agx-offscreen-test.nix)
  D=$ROOT/runs/m1-desktop-010/wm-darwin-scale-desktop-build-009/target/aarch64-apple-darwin/release/chonkstep-darwin-desktop
  TERM_BIN=$ROOT/runs/m1-desktop-010/terminal-application-build-008/target/aarch64-apple-darwin/release/chonk-terminal-darwin
  [ -x "$D" ] && [ -x "$TERM_BIN" ] || { echo "no compositor or terminal binary" >&2; exit 2; }
  X="$D=/usr/bin/chonkstep-darwin-desktop;$TERM_BIN=/usr/bin/chonk-terminal-darwin"
  TI=$ROOT/integrations/puredarwin/chonkstep-darwin/terminfo/xterm-256color
  [ -r "$TI" ] && X="$X;$TI=/usr/share/terminfo/x/xterm-256color;$TI=/usr/share/terminfo/78/xterm-256color"
  BG=$ROOT/integrations/puredarwin/chonkstep-darwin/backgrounds/siliconforest.png
  [ -f "$BG" ] && X="$X;$BG=/usr/share/backgrounds/siliconforest/siliconforest.png"
  for l in libEGL.1.dylib libGLESv2.2.dylib libgallium-26.3.0-devel.dylib libdrm.2.dylib; do X="$X;$T/usr/local/lib/mesa-asahi/$l=/usr/local/lib/mesa-asahi/$l"; done
  A=$($NP m1-sound/sysaudio build-sysaudio.nix -A all); PLAY=$($NP m1-sound build-pdplay.nix)
  [ -x "$A/usr/sbin/coreaudiod" ] || { echo "sysaudio build failed" >&2; exit 2; }
  FW=System/Library/Frameworks
  X="$X;$A/usr/sbin/coreaudiod=/usr/sbin/coreaudiod;$A/usr/bin/pdtone=/usr/bin/pdtone;$A/usr/bin/pdvolume=/usr/bin/pdvolume;$PLAY/usr/bin/pdplay=/usr/bin/pdplay"
  for f in CoreAudio AudioToolbox AudioUnit; do X="$X;$A/$FW/$f.framework/Versions/A/$f=/$FW/$f.framework/Versions/A/$f"; done
  TBFW=$ROOT/private/firmware/apple/dfrmtfw-j293.bin
  [ -f "$TBFW" ] && X="$X;$TBFW=/Library/Firmware/apple/dfrmtfw-j293.bin"
  if [ -f "$BG" ]; then
    python3 integrations/puredarwin/m1-quiet-boot/boot_logo.py pack "$BG" "$QD" > /dev/null
    FILES="$FILES;$QD/boot-logo.gz=$B/boot-logo.gz;$QD/boot-logo.json=$B/boot-logo.json"
  fi
  # Nobody listens: the speakers stay 20 dB under the protection model's limit.
  sed -e "s/@SECONDS@/${NB_SECONDS:-30}/g" -e "s/@HOLD@/1/g" -e "s/@VOLUME@/30/g" integrations/puredarwin/m1-boot-speed/guest/darwin-gpu-desktop-fast.in > "$S"
  ARGS="cpus=${NB_CPUS:-8} pdgpudt=2 pdgpudt_stringprops=1 pdagx=1 pddispdt=2 pddcp=2 $WAITS ${NB_BOOTARGS:-}"
else
  cp "$HERE/guest-shell.in" "$S"
  ARGS="cpus=${NB_CPUS:-1} $WAITS ${NB_BOOTARGS:-}"
fi
R=runs/m1-desktop-010/pcie-enum-attempt-$N
env PCIE_LAB_PATCH="$HERE/launcher-native.patch" PCIE_LAB_FILES="$FILES" \
  PCIE_STAGER="$ROOT/integrations/puredarwin/m1-boot-speed/tools/stage_ramdisk_slim.py" \
  PCIE_KC="$KC" PCIE_BOOTARGS="$ARGS" PCIE_EXTRA_FILES="$X" PCIE_GUEST_TEMPLATE="$S" PCIE_DONE=$DONE \
  PCIE_USB_CHUNK=1048576 PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "$LIMIT" 2>&1 | grep -a '"status"\|rror\|delivered\|No space\|BENCH_TIMES' | cut -c1-240 || true
[ -f "$R/times.txt" ] || { echo "no boot was recorded in $R" >&2; exit 1; }
printf 'mode=%s\nprofile=%s\nkc=%s\nbootargs=%s\nlimit=%s\n' "$WORDS" "$PROFILE" "$KC" "$ARGS" "$LIMIT" > "$R/nb-config.txt"
cat "$R/result.json" 2>/dev/null || true
grep -a 'NATIVE\|UART_TAP\|UART_MARKER\|GUEST_CPUS' "$R/guest/worker.log" | cut -c1-400 || true
grep -a -c '^UART> ' "$R/guest/worker.log" | sed 's/^/UART lines in worker.log: /' || true
strings -n 4 "$R/guest/console.bin" | grep -a 'PDNB\|panic\|Darwin Kernel\|DL9_\|GPUDESK\|WMSTAT' | cut -c1-240 | head -60 || true
}
main "$@"; exit $?
