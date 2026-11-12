#!/bin/bash
# SPDX-License-Identifier: MIT
# Work on the SiliconForest partition from the M1's Linux, in ONE ssh session, on that
# partition ONLY. Before anything it is identified three ways and the session ends if one
# does not hold: PARTUUID and label (blkid), first block, length (sysfs), against
# private/disk-root/sf-part.env; it must not be mounted. Nothing else on the disk is opened
# for writing. Run it through the bench: BENCH_WHO=disk tools/bench.sh run .../dr_linux_root.sh <cmd>
#   check                  identity, e2fsck -fn, usage
#   transform <sf-roinit>  mount, tools/sf-root-transform.sh on it, unmount, e2fsck -fn
#   tree                   mount read-only (noload), a listing digest, unmount
# Output: runs/m1-storage/linux-root-<cmd>-<time>.txt
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../../.." && pwd); cd "$ROOT"
. private/disk-root/sf-part.env
CMD=${1:?check|transform|tree}; INIT=${2:-}
M1="ssh -o HostKeyAlias=10.1.1.127 -o BatchMode=yes -o ConnectTimeout=8 -o ServerAliveInterval=15 chrisk@10.1.1.211"
OUT=runs/m1-storage/linux-root-$CMD-$(date +%Y%m%d-%H%M%S).txt
for i in $(seq 1 36); do $M1 true 2>/dev/null && break; sleep 5; done
{
  echo "WANT_PARTUUID=$SF_PARTUUID WANT_LBA=$SF_PART_LBA WANT_BLOCKS=$SF_PART_BLOCKS CMD=$CMD"
  if [ "$CMD" = transform ]; then
    [ -f "$INIT" ] || { echo "transform needs the sf-roinit binary" >&2; exit 2; }
    echo "INIT_B64=$(base64 -w0 "$INIT")"; echo "TRANSFORM_B64=$(base64 -w0 "$HERE/sf-root-transform.sh")"
  fi
  cat <<'REMOTE'
set -u
P=/dev/nvme0n1p3; S=/sys/block/nvme0n1/nvme0n1p3; MNT=/run/sf-dst
fail() { echo "SFL_FAIL $1"; exit 1; }
[ "$(blkid -s PARTUUID -o value $P)" = "$WANT_PARTUUID" ] || fail "PARTUUID of $P is not ours"
[ "$(blkid -s LABEL -o value $P)" = SiliconForest ] || fail "label"
[ "$(( $(cat $S/start) / 8 ))" = "$WANT_LBA" ] || fail "first block"
[ "$(( $(cat $S/size) / 8 ))" = "$WANT_BLOCKS" ] || fail "length"
findmnt -rn -S $P >/dev/null && fail "it is mounted"
echo "SFL_IDENTITY ok (PARTUUID, label, first block, length; not mounted)"
case "$CMD" in
check)
  e2fsck -fn $P 2>&1 | tail -3 | sed 's/^/SFL_FSCK /'; echo "SFL_FSCK_RC ${PIPESTATUS[0]}";;
transform)
  T=$(mktemp -d); echo "$INIT_B64" | base64 -d > $T/sf-roinit; echo "$TRANSFORM_B64" | base64 -d > $T/transform.sh
  mkdir -p $MNT && mount -t ext4 $P $MNT || fail "mount"
  sh $T/transform.sh $MNT $T/sf-roinit 2>&1 | sed 's/^/SFL_TRANSFORM /'; rc=${PIPESTATUS[0]}
  ls -ld $MNT/var $MNT/private/var $MNT/private/tmp $MNT/private/ram $MNT/etc/resolv.conf $MNT/usr/appleinternal/sbin/launchd.sfro 2>&1 | sed 's/^/SFL_LS /'
  sync; umount $MNT || fail "umount"; rm -rf $T
  echo "SFL_TRANSFORM_RC $rc"
  e2fsck -fn $P 2>&1 | tail -3 | sed 's/^/SFL_FSCK /'; echo "SFL_FSCK_RC ${PIPESTATUS[0]}";;
tree)
  mkdir -p $MNT && mount -t ext4 -o ro,noload $P $MNT || fail "mount"
  echo "SFL_TREE entries=$(cd $MNT && find . | wc -l) listing=$(cd $MNT && find . -printf '%y %p %s %l\n' | grep -v '^d ' | LC_ALL=C sort | sha256sum | cut -c1-64)"
  df -k $MNT | tail -1 | sed 's/^/SFL_DF /'
  umount $MNT || fail "umount";;
esac
dumpe2fs -h $P 2>/dev/null | grep -iE 'state|Mount count|features' | sed 's/^/SFL_SB /'
dmesg | grep -iE 'EXT4-fs.*nvme0n1p3' | tail -4 | sed 's/^/SFL_DMESG /'
echo SFL_END
REMOTE
} | sed -e '1s/^/export /' -e 's/^\(INIT_B64\|TRANSFORM_B64\)=/export &/' | $M1 'sudo -n bash -s' > "$OUT" 2>&1 || echo "ssh session ended with $?" >&2
grep -a "^SFL_" "$OUT" | cut -c1-200
grep -q "^SFL_END" "$OUT"
