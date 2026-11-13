#!/bin/bash
# SPDX-License-Identifier: MIT
# Put a new sf/boot.bin (stage 2 + configuration + kernel collection, ../build.sh) on the
# SiliconForest boot partition, from the M1's Linux. This is the whole kernel-update path:
# no recovery mode, no macOS; stage 1 in the startup volume stays as it is and reads the new
# file at the next start of SiliconForest.
#   sudo ./sf-bootbin-install.sh <PARTUUID of the boot partition> <boot.bin>
#   sudo ./sf-bootbin-install.sh <PARTUUID of the boot partition> --rollback     (the previous file back)
#   sudo ./sf-bootbin-install.sh <PARTUUID of the boot partition> --show         (what is there; read-only mount)
# The PARTUUID is the one the installer printed ("EFI PARTUUID") and stage 1 carries in its
# chainload= line; it is not Linux's EFI partition, and the script refuses that one.
# It writes ONLY inside that partition's FAT filesystem: the new file under a temporary
# name, synced and compared, then two renames (boot.bin -> boot.bin.old, new -> boot.bin).
# A power cut between the renames leaves boot.bin.old and boot.bin.new: stage 1 then stops
# with its message (hold the power button; Linux starts as before), and running this again repairs it.
set -euo pipefail
ID=${1:?PARTUUID of the boot partition}; WHAT=${2:?boot.bin, --rollback or --show}
[ "$(id -u)" = 0 ] || { echo "run with sudo" >&2; exit 2; }
ID=$(echo "$ID" | tr 'A-F' 'a-f')
DEV=$(blkid -t PARTUUID="$ID" -o device | head -1); [ -b "$DEV" ] || { echo "no partition with that PARTUUID" >&2; exit 2; }
refuse() { echo "REFUSED: $*" >&2; exit 3; }
[ "$(blkid -s TYPE -o value "$DEV")" = vfat ] || refuse "$DEV is not FAT"
case "$(blkid -s LABEL -o value "$DEV")" in SFBOOT*) ;; *) refuse "$DEV is not labelled SFBOOT";; esac
[ "$(lsblk -n -o PARTTYPE "$DEV" | tr 'A-F' 'a-f')" = c12a7328-f81f-11d2-ba4b-00a0c93ec93b ] || refuse "$DEV has another partition type than the installer gave it"
[ "$(blockdev --getsize64 "$DEV")" -le $((600 << 20)) ] || refuse "$DEV is larger than our boot partition"
! findmnt -rn -S "$DEV" > /dev/null || refuse "$DEV is mounted"
! grep -i -q "$ID\|$(blkid -s UUID -o value "$DEV")" /etc/fstab || refuse "$DEV is named in /etc/fstab: it belongs to Linux"
[ ! -d "/sys/firmware/devicetree/base/chosen" ] || ! tr -d '\0' < /sys/firmware/devicetree/base/chosen/asahi,efi-system-partition 2>/dev/null | grep -i -q "$ID" || refuse "$DEV is the EFI partition this Linux was booted from"
MNT=$(mktemp -d); trap 'umount "$MNT" 2>/dev/null || true; rmdir "$MNT"' EXIT
if [ "$WHAT" = --show ]; then
  mount -t vfat -o ro "$DEV" "$MNT"; ls -la "$MNT" "$MNT/sf" 2>/dev/null || true; sha256sum "$MNT"/sf/* 2>/dev/null || true; exit 0
fi
if [ "$WHAT" = --rollback ]; then
  mount -t vfat -o rw,noatime "$DEV" "$MNT"
  [ -f "$MNT/sf/boot.bin.old" ] || { echo "no previous file" >&2; exit 2; }
  [ ! -f "$MNT/sf/boot.bin" ] || mv "$MNT/sf/boot.bin" "$MNT/sf/boot.bin.new"
  mv "$MNT/sf/boot.bin.old" "$MNT/sf/boot.bin"; [ ! -f "$MNT/sf/boot.bin.new" ] || mv "$MNT/sf/boot.bin.new" "$MNT/sf/boot.bin.old"
  sync -f "$MNT"; sha256sum "$MNT"/sf/*; echo "SFBOOT rolled back"; exit 0
fi
[ -f "$WHAT" ] || { echo "no file $WHAT" >&2; exit 2; }
# The file: stage m1n1, then the records with their checksums, nothing else; at most 60 MiB
# (what a stage 1 can chainload); built for the root that is on this disk.
ROOT=$(blkid -L SiliconForest || true); [ -b "$ROOT" ] || refuse "no partition labelled SiliconForest on this machine"
python3 -B - "$WHAT" "$(blkid -s UUID -o value "$ROOT")" <<'PY' || refuse "the file is not a SiliconForest boot image for this machine"
import struct, sys, zlib
data, root = open(sys.argv[1], 'rb').read(), sys.argv[2]
assert len(data) <= 60 << 20, 'larger than 60 MiB'
assert data[0x808:0x810] == b'AURALOAD', 'no m1n1 load header'
at, kinds = struct.unpack_from('<Q', data, 0x818)[0], []
while True:
    magic, version, kind, flags, crc, stored, raw = struct.unpack_from('<IHHIIQQ', data, at); at += 32
    assert magic == 0x46534450 and version == 1, 'not a record'
    if kind == 0: break
    body = data[at:at + stored]; assert len(body) == stored and zlib.crc32(body) == crc, 'checksum'
    kinds.append(kind)
    if kind == 1: text = body.decode('ascii')
    at += stored + (-(32 + stored) % 16)
assert kinds == [1, 2] and data[at:] == bytes(4), 'not stage + configuration + kernel'
args = text.splitlines()[0].split()
assert 'rd=uuid' in args and ('boot-uuid=' + root) in args, 'built for another root'
assert b'SF_STOPPED' in data[:struct.unpack_from('<Q', data, 0x818)[0]], 'the bench variant of the stage (it would open a USB proxy on failure)'
assert not any(a.startswith(('sfbench', 'sfmark', 'sfsession_seconds', 'pdsfstop')) for a in args), 'a bench image'
print('SFBOOT file ok: %d bytes, records config + kernel, root %s...' % (len(data), root[:4]))
PY
mount -t vfat -o rw,noatime "$DEV" "$MNT"
mkdir -p "$MNT/sf"
[ "$(df --output=avail -B1 "$MNT" | tail -1)" -gt $(( $(stat -c %s "$WHAT") + (8 << 20) )) ] || refuse "not enough room (remove sf/boot.bin.old by hand if it is not wanted)"
rm -f "$MNT/sf/boot.bin.new"
cp "$WHAT" "$MNT/sf/boot.bin.new"; sync -f "$MNT"
cmp "$WHAT" "$MNT/sf/boot.bin.new" || refuse "the copy differs; boot.bin was not replaced"
[ ! -f "$MNT/sf/boot.bin" ] || mv -f "$MNT/sf/boot.bin" "$MNT/sf/boot.bin.old"
mv "$MNT/sf/boot.bin.new" "$MNT/sf/boot.bin"; sync -f "$MNT"
umount "$MNT"; mount -t vfat -o ro "$DEV" "$MNT"
cmp "$WHAT" "$MNT/sf/boot.bin" || refuse "after the remount sf/boot.bin differs from the file: use --rollback"
ls -la "$MNT/sf"; sha256sum "$MNT/sf/boot.bin"
echo "SFBOOT updated: SiliconForest starts this file the next time it is chosen. The previous one is sf/boot.bin.old (--rollback)."
