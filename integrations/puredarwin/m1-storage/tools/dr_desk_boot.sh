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
case "$MODE" in hv) WORDS=;; native) WORDS="native vdm-return vdm-serial";; *) echo "hv or native" >&2; exit 2;; esac
. private/disk-root/sf-part.env
"$HERE/host-test/run-host-tests.sh" > /dev/null || { echo "host tests failed; not booting" >&2; exit 2; }
NP=integrations/puredarwin/m1-boot-speed/tools/bs_nix_path.py
KC=$(env -u SF_PD $NP m1-storage build-kc-native.nix); [ -f "$KC/kernel" ] || { echo "no collection" >&2; exit 2; }
TEST=$($NP m1-storage build-pdnvme-test.nix); [ -x "$TEST/usr/bin/pdnvme-test" ] || { echo "no pdnvme-test" >&2; exit 2; }
B=integrations/puredarwin/m1-bringup
QD=$(mktemp -d); S=$(mktemp); trap 'rm -rf "$QD" "$S"' EXIT
echo "$WORDS" > "$QD/native_mode.txt"
python3 -B -c "import sys; sys.path.insert(0, '$NB'); import native_boot; native_boot.Mode(open('$QD/native_mode.txt').read())"
cat "$NB/launcher-native.patch" "$HERE/launcher-diskroot.patch" > "$QD/launcher.patch"
FILES="$NB/native_boot.py=$B/native_boot.py;$NB/native_stub.py=$B/native_stub.py;$QD/native_mode.txt=$B/native_mode.txt"
TTL=${DR_TTL:-$((LIMIT - 40))}
DISK="rd=uuid boot-uuid=$SF_FSUUID launchdsuffix=sfro wdt=-1 pdnvme=3 pdnvme_ttl=$TTL pdnvme_part=$SF_PARTUUID pdnvme_part_lba=$SF_PART_LBA pdnvme_part_blocks=$SF_PART_BLOCKS pdnvme_readfence=1 pd_quiet_wait=0 pd_rtc_wait=0"
[ -f "${DR_RANGES:-}" ] || { echo "DR_RANGES=<range file> is needed" >&2; exit 2; }
sed -e "s/@PART_LBA@/$SF_PART_LBA/g" -e "s/@PART_BLOCKS@/$SF_PART_BLOCKS/g" "$HERE/guest/darwin-diskroot.in" > "$QD/dr-diskroot-end"
X="$TEST/usr/bin/pdnvme-test=/usr/bin/pdnvme-test;$DR_RANGES=/etc/pdnvme-ranges;$QD/dr-diskroot-end=/usr/bin/dr-diskroot-end"
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
  { sed -n '1,/^nonce=/p' "$G"; cat "$HERE/guest/diskroot-prelude.sh"; sed -e '1,/^nonce=/d' -e '$ d' "$G"; cat "$HERE/guest/diskroot-end.sh"; } |
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
printf 'mode=%s\nprofile=%s\nkc=%s\nlimit=%s\n' "$MODE" "${DR_PROFILE:-desktop}" "$KC" "$LIMIT" > "$R/dr-config.txt"
grep -o '"status": *"[^"]*"' "$R/result.json" 2>/dev/null || echo "no result.json"
