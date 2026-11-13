#!/bin/bash
# SPDX-License-Identifier: MIT
# One boot of the default desktop with the ROOT ON THE SILICONFOREST PARTITION, read-only
# (disk-root stream, step 2), under the hypervisor or natively. It is ../../m1-native-boot/
# tools/nb_boot.sh's desktop profile (same guest script, same staged files, same launcher
# patch) with: the native collection plus PDAppleNVMe (../build-kc-native.nix), the launcher's
# rd=md0 replaced (../launcher-diskroot.patch), the root named by filesystem UUID, process 1
# = the wrapper that mounts the memory disk for /var and /tmp, the read fence on, and the
# script's end replaced by a clean controller stop. The RAM root is uploaded as always: it is
# the memory disk, it carries this boot's script, and it is the root the boot can be repeated
# with. Programs, libraries and the desktop itself are read from the disk.
# Usage: dr_desk_boot.sh <name, prefix dr> <hv|native> [upper bound s, 120..600, default 200]
#   DR_SECONDS  session length (default 30);  DR_TTL  driver time to live (default bound - 40)
#   DR_RANGES   the range file for the hashes (required);  DR_PROFILE=shell  no desktop, only ../guest/darwin-diskroot.in
#   DR_BOOTARGS further boot arguments
#   DR_RW=1     step 3d: the root READ-WRITE through the write-fenced build (../build-kc-native-rw.nix:
#               PDAppleNVMeRW, ext4 with the downgrade patch, six other-board kexts left out and
#               ../launcher-m1-members.patch for the launcher's required list); process 1 is launchd
#               itself; the partition must have been prepared with tools/dr_linux_root.sh untransform.
#               The boot ends: sync, root downgraded to read-only, fence sealed, controller stopped
#   DR_HALT="<command>"  (with DR_RW=1, native only) the boot is ended by the SYSTEM'S OWN command, e.g.
#               DR_HALT=reboot or DR_HALT="shutdown -r now" (../system: sf-command -> sf-halt), or DR_HALT=menu
#               (the compositor's autopilot clicks Session > Restart in the desktop menu; the script then runs
#               `shutdown -r now` because the session asked for it), not by the
#               guest script: collection ../build-kc-native-rw-halt.nix (PDCpuPower with the restart through the
#               SMC and the hardware clock), the launcher's rule for a guest that restarts the machine itself
#               (../../m1-native-boot/reboot/launcher-self-reboot.diff: no controller reset when the M1's Linux
#               comes back by itself), guests ../guest/diskroot-halt-prelude.sh and ../guest/darwin-diskroot-halt.in
#   DR_INSTALL=1  (with DR_HALT) the boot carries ../system and tools/staged.sh's compositor (DR_COMPOSITOR=<file>:
#               another) in its RAM root and its script installs them on the disk root (through the fenced driver)
#   DR_AUTO_ONLY=<phases>  the compositor's autopilot runs these phases (e.g. settle,settings); DR_AUTO_KEEP=1:
#               its settings phase picks another theme and display scale from the menu and leaves them
# Run inside a slot (Linux before and after): tools/dr_desk_slot.sh.
set -euo pipefail
HERE=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd); ROOT=$(cd "$HERE/../../.." && pwd); cd "$ROOT"
NB=$ROOT/integrations/puredarwin/m1-native-boot
export PATH=$HOME/.nix-profile/bin:$PATH BENCH_WHO=${BENCH_WHO:-disk} SF_PD=pin
N=${1:?name}; MODE=${2:?hv or native}; LIMIT=${3:-200}
case "$N" in dr[0-9]*) ;; *) echo "boot names are dr01, dr02, ..." >&2; exit 2;; esac
[ "$LIMIT" -ge 120 ] && [ "$LIMIT" -le 600 ] || { echo "the upper bound is 120..600 seconds" >&2; exit 2; }
[ ! -e "runs/m1-desktop-010/pcie-enum-attempt-$N" ] || { echo "boot $N exists" >&2; exit 2; }
[ ! -e runs/bench.finisher.failed ] && [ ! -e runs/m1-storage/STOP ] || { echo "a stop file exists" >&2; exit 3; }
[ -z "${PCIE_CHAIN:-}" ] || { echo "PCIE_CHAIN is not used in this stream" >&2; exit 2; }
# noiolog: driver log lines on the 115200-baud UART delay the display driver until the compositor
# has given up on GL (boot dr07 came up on the software renderer without it)
case "$MODE" in hv) WORDS=;; native) WORDS="native vdm-return vdm-serial noiolog";; *) echo "hv or native" >&2; exit 2;; esac
. private/disk-root/sf-part.env
"$HERE/host-test/run-host-tests.sh" > /dev/null || { echo "host tests failed; not booting" >&2; exit 2; }
NP=integrations/puredarwin/m1-boot-speed/tools/bs_nix_path.py
RW=${DR_RW:-0}; HALT=${DR_HALT:-}
if [ -n "$HALT" ]; then
  [ "$RW" = 1 ] && [ "$MODE" = native ] || { echo "DR_HALT needs DR_RW=1 and a native boot" >&2; exit 2; }
  case "$HALT" in reboot|"shutdown -r now"|menu) ;; *) echo "DR_HALT is reboot, \"shutdown -r now\" or menu (a power-off needs a person at the machine)" >&2; exit 2;; esac
  [ "$HALT" != menu ] || case ",${DR_AUTO_ONLY:-}," in *,settings,*) ;; *) echo "DR_HALT=menu needs DR_AUTO_ONLY with the settings phase" >&2; exit 2;; esac
fi
KC=$(env -u SF_PD $NP m1-storage build-kc-native$([ "$RW" = 1 ] && echo -rw)$([ -n "$HALT" ] && echo -halt).nix); [ -f "$KC/kernel" ] || { echo "no collection" >&2; exit 2; }
TEST=$($NP m1-storage build-pdnvme-test.nix); [ -x "$TEST/usr/bin/pdnvme-test" ] || { echo "no pdnvme-test" >&2; exit 2; }
B=integrations/puredarwin/m1-bringup
QD=$(mktemp -d); S=$(mktemp); trap 'rm -rf "$QD" "$S"' EXIT
echo "$WORDS" > "$QD/native_mode.txt"
python3 -B -c "import sys; sys.path.insert(0, '$NB'); import native_boot; native_boot.Mode(open('$QD/native_mode.txt').read())"
if [ -n "$HALT" ]; then
  # launcher-native.patch with the self-restart rules on top, as one patch (both change the same two files)
  "$NB/reboot/combine_patch.sh" "$QD/launcher-native-self-reboot.patch"
  cat "$QD/launcher-native-self-reboot.patch" "$HERE/launcher-diskroot.patch" > "$QD/launcher.patch"
  echo "the guest of boot $N restarts the M1 itself" > "$QD/self_reboot_mode.txt"
else
  cat "$NB/launcher-native.patch" "$HERE/launcher-diskroot.patch" > "$QD/launcher.patch"
fi
[ "$RW" != 1 ] || cat "$HERE/launcher-m1-members.patch" >> "$QD/launcher.patch"
FILES="$NB/native_boot.py=$B/native_boot.py;$NB/native_stub.py=$B/native_stub.py;$QD/native_mode.txt=$B/native_mode.txt"
[ -z "$HALT" ] || FILES="$FILES;$QD/self_reboot_mode.txt=$B/self_reboot_mode.txt"
TTL=${DR_TTL:-$((LIMIT - 40))}
DISK="rd=uuid boot-uuid=$SF_FSUUID launchdsuffix=sfro wdt=-1 pdnvme=3 pdnvme_ttl=$TTL pdnvme_part=$SF_PARTUUID pdnvme_part_lba=$SF_PART_LBA pdnvme_part_blocks=$SF_PART_BLOCKS pdnvme_readfence=1 pd_quiet_wait=0 pd_rtc_wait=0"
[ -f "${DR_RANGES:-}" ] || { echo "DR_RANGES=<range file> is needed" >&2; exit 2; }
END=$HERE/guest/darwin-diskroot.in; PRELUDE=$HERE/guest/diskroot-prelude.sh
if [ "$RW" = 1 ]; then
  DISK="rd=uuid boot-uuid=$SF_FSUUID wdt=-1 pdnvme=3 pdnvme_ttl=$TTL pdnvme_part=$SF_PARTUUID pdnvme_part_lba=$SF_PART_LBA pdnvme_part_blocks=$SF_PART_BLOCKS pdnvme_write=1 pdnvme_readfence=1 pd_quiet_wait=0 pd_rtc_wait=0"
  END=$HERE/guest/darwin-diskroot-rw.in; PRELUDE=$HERE/guest/diskroot-rw-prelude.sh
fi
if [ -n "$HALT" ]; then
  # pdthermal_delay: PDCpuPower's first SMC access (default 20 s into the boot), which is also when the clock is read
  DISK="$DISK pdthermal_delay=${DR_THERMAL_DELAY:-4}"
  END=$HERE/guest/darwin-diskroot-halt.in; PRELUDE=$HERE/guest/diskroot-halt-prelude.sh
fi
sed -e "s/@PART_LBA@/$SF_PART_LBA/g" -e "s/@PART_BLOCKS@/$SF_PART_BLOCKS/g" -e "s/@NAME@/$N/g" -e "s/@HALT_COMMAND@/$HALT/g" "$END" > "$QD/dr-diskroot-end"
X="$TEST/usr/bin/pdnvme-test=/usr/bin/pdnvme-test;$DR_RANGES=/etc/pdnvme-ranges;$QD/dr-diskroot-end=/usr/bin/dr-diskroot-end"
if [ -n "$HALT" ] && [ "${DR_INSTALL:-0}" = 1 ]; then
  SYS=$(env -u SF_PD $NP m1-storage build-system-tools.nix); [ -x "$SYS/sbin/sf-halt" ] || { echo "no system tools" >&2; exit 2; }
  COMP=${DR_COMPOSITOR:-$(env -u SF_PD tools/staged.sh compositor)}; [ -x "$COMP" ] || { echo "no compositor at $COMP" >&2; exit 2; }
  X="$X;$SYS/sbin/sf-halt=/usr/share/sf-system/sf-halt;$SYS/usr/sbin/sf-clock=/usr/share/sf-system/sf-clock;$COMP=/usr/bin/chonkstep-darwin-desktop"
  for f in sf-boot sf-command sf-install.sh darwin-desktop-test.disk; do X="$X;$HERE/system/$f=/usr/share/sf-system/$f"; done
fi
if [ "${DR_PROFILE:-desktop}" = desktop ]; then
  BG=$ROOT/integrations/puredarwin/chonkstep-darwin/backgrounds/siliconforest.png
  if [ -f "$BG" ]; then
    python3 integrations/puredarwin/m1-quiet-boot/boot_logo.py pack "$BG" "$QD" > /dev/null
    FILES="$FILES;$QD/boot-logo.gz=$B/boot-logo.gz;$QD/boot-logo.json=$B/boot-logo.json"
  fi
  # the desktop's own guest script (speakers held 20 dB down: nobody listens), with facts about
  # the root in front and, instead of its end marker, the stop of the controller from memory
  G=integrations/puredarwin/m1-boot-speed/guest/darwin-gpu-desktop-fast.in
  [ "$(tail -n 1 "$G")" = "printf '\\nPD_BENCH_DONE\\n'" ] || { echo "the desktop script does not end with the end marker" >&2; exit 2; }
  { sed -n '1,/^nonce=/p' "$G"; sed -e "s/@NAME@/$N/g" -e "s/@INSTALL@/${DR_INSTALL:-0}/g" -e "s/@AUTO_ONLY@/${DR_AUTO_ONLY:-}/g" -e "s/@AUTO_KEEP@/${DR_AUTO_KEEP:-0}/g" -e "s/@AUTO_END@/$([ "$HALT" = menu ] && echo restart)/g" "$PRELUDE"
    sed -e '1,/^nonce=/d' -e '$ d' "$G"; cat "$HERE/guest/diskroot-end.sh"; } |
    sed -e "s/@SECONDS@/${DR_SECONDS:-30}/g" -e "s/@HOLD@/1/g" -e "s/@VOLUME@/30/g" > "$S"
  ARGS="cpus=8 pdgpudt=2 pdgpudt_stringprops=1 pdagx=1 pddispdt=2 pddcp=2 $DISK ${DR_BOOTARGS:-}"
else
  cp "$QD/dr-diskroot-end" "$S"
  ARGS="cpus=8 $DISK ${DR_BOOTARGS:-}"
fi
case "$MODE" in native) ARGS="$ARGS pdcpuheld=1";; esac
R=runs/m1-desktop-010/pcie-enum-attempt-$N
env PCIE_LAB_PATCH="$QD/launcher.patch" PCIE_LAB_FILES="$FILES" \
  PCIE_STAGER="$ROOT/integrations/puredarwin/m1-boot-speed/tools/stage_ramdisk_slim.py" \
  PCIE_KC="$KC" PCIE_BOOTARGS="$ARGS" PCIE_EXTRA_FILES="$X" PCIE_GUEST_TEMPLATE="$S" PCIE_DONE=marker \
  PCIE_USB_CHUNK=1048576 PCIE_SKIP_BUILD=1 PCIE_WIFI=1 PCIE_WIFI_JOIN=1 LINES_MAX=1 \
  tools/pcie_iterate.sh "$N" "$LIMIT" 2>&1 | grep -a '"status"\|rror\|delivered\|No space\|BENCH_TIMES' | cut -c1-240 || true
[ -f "$R/times.txt" ] || { echo "no boot was recorded in $R" >&2; exit 1; }
printf 'mode=%s\nprofile=%s\nkc=%s\nlimit=%s\nrw=%s\nhalt=%s\ninstall=%s\nauto=%s keep=%s\n' "$MODE" "${DR_PROFILE:-desktop}" "$KC" "$LIMIT" "$RW" "$HALT" "${DR_INSTALL:-0}" "${DR_AUTO_ONLY:-}" "${DR_AUTO_KEEP:-0}" > "$R/dr-config.txt"
grep -o '"status": *"[^"]*"' "$R/result.json" 2>/dev/null || echo "no result.json"
