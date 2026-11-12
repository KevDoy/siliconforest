#!/bin/bash
# SPDX-License-Identifier: MIT
# READ-ONLY fact gathering on the M1's Linux for the storage work. Every remote command
# only reads: no file is created on the M1 (output comes back over ssh), the disk is opened
# read-only (dd if=... iflag=direct piped to sha256sum), nothing is mounted, written,
# trimmed or formatted. Results go to runs/m1-storage/<tag>/ (never committed: they hold
# serial numbers and partition identities).
#   linux_m1.sh facts <tag>     lsblk, NVMe identify and SMART, kernel log, device tree, filesystem state
#   linux_m1.sh ranges <tag>    writes <tag>/ranges: "<lba> <blocks>" in 4096-byte blocks, spread over the disk
#   linux_m1.sh hash <tag> <rangefile>   SHA-256 of each range as Linux reads it -> <tag>/hashes
#   linux_m1.sh compare <a> <b>          two hash files: equal / different ranges
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
M1="ssh -o HostKeyAlias=10.1.1.127 -o BatchMode=yes -o ConnectTimeout=8 chrisk@10.1.1.211"
DISK=nvme0n1
cmd=${1:?facts|ranges|hash|compare}
if [ "$cmd" = compare ]; then
  join -j1 <(awk '/^NVT_HASH [0-9]/ {print $2"+"$3, $4}' "$2" | sort) <(awk '/^NVT_(MEDIA_)?HASH [0-9]/ {print $2"+"$3, $4}' "$3" | sort) |
    awk '{ if ($2 == $3) e++; else { d++; print "DIFFERENT", $1 } } END { printf "ranges equal=%d different=%d\n", e, d }'
  exit 0
fi
OUT=$ROOT/runs/m1-storage/${2:?tag}; mkdir -p "$OUT"
case "$cmd" in
facts)
  $M1 'uname -r; cat /proc/sys/kernel/random/boot_id; uptime' > "$OUT/uname.txt"
  $M1 "lsblk -b -o NAME,SIZE,TYPE,FSTYPE,RO,MOUNTPOINTS,PHY-SEC,LOG-SEC,PARTTYPENAME /dev/nvme0n1 /dev/nvme0n2 /dev/nvme0n3 2>&1" > "$OUT/lsblk.txt" || true
  $M1 "for p in /sys/block/$DISK /sys/block/$DISK/${DISK}p*; do echo \$(basename \$p) start=\$(cat \$p/start 2>/dev/null || echo 0) sectors=\$(cat \$p/size) ro=\$(cat \$p/ro); done; cat /sys/block/$DISK/queue/logical_block_size /sys/block/$DISK/queue/max_hw_sectors_kb /sys/block/$DISK/queue/nr_requests" > "$OUT/sysfs.txt"
  $M1 'sudo -n nvme id-ctrl /dev/nvme0 2>&1' > "$OUT/id-ctrl.txt" || true
  $M1 'sudo -n nvme id-ns /dev/nvme0n1 2>&1' > "$OUT/id-ns.txt" || true
  $M1 'sudo -n nvme smart-log /dev/nvme0 2>&1' > "$OUT/smart-log.txt" || true
  $M1 'sudo -n nvme list-ns /dev/nvme0 2>&1' > "$OUT/list-ns.txt" || true
  $M1 "sudo -n dmesg 2>&1 | grep -iE 'nvme|sart|ans|rtkit|EXT4|BTRFS|XFS|F2FS|recover|journal|orphan|fsck|I/O error'" > "$OUT/dmesg.txt" || true
  $M1 "journalctl -k -b --no-pager 2>/dev/null | grep -iE 'recover|replay|orphan|unclean|not cleanly|corrupt|I/O error|nvme' | tail -60" > "$OUT/journal-fs.txt" || true
  $M1 'findmnt -rn -o TARGET,SOURCE,FSTYPE,OPTIONS | grep -E "nvme|/boot|^/ "; cat /proc/swaps' > "$OUT/mounts.txt" || true
  $M1 'for n in /proc/device-tree/soc/nvme@* /proc/device-tree/soc/iommu@27bc50000 /proc/device-tree/soc/mbox@277408000; do echo "== $n"; for f in compatible reg reg-names interrupts power-domain-names; do [ -e $n/$f ] && { printf "%s: " $f; od -An -tx1 $n/$f | tr -d "\n"; echo; }; done; done' > "$OUT/devicetree.txt" || true
  $M1 'cat /sys/class/nvme/nvme0/model /sys/class/nvme/nvme0/firmware_rev /sys/class/nvme/nvme0/state /sys/class/nvme/nvme0/queue_count /sys/class/nvme/nvme0/sqsize 2>&1; grep -i nvme /proc/interrupts' > "$OUT/nvme-sysfs.txt" || true
  ls -la "$OUT";;
ranges)
  $M1 "for p in /sys/block/$DISK /sys/block/$DISK/${DISK}p*; do echo \$(cat \$p/start 2>/dev/null || echo 0) \$(cat \$p/size); done" |
  awk 'NR == 1 { total = $2 / 8; print 0, 256; for (i = 1; i < 8; i++) print int(total * i / 8 / 256) * 256 + 4096, 256; print total - 256, 256; next }
       { s = $1 / 8; n = $2 / 8; if (n >= 768) { print s, 256; print s + int(n / 2 / 256) * 256, 256; print s + n - 256, 256 } else print s, n }' |
  sort -n -u > "$OUT/ranges"
  wc -l "$OUT/ranges";;
hash)
  R=${3:?range file}
  : > "$OUT/hashes"
  while read -r lba blocks; do
    h=$($M1 "sudo -n dd if=/dev/$DISK bs=4096 skip=$lba count=$blocks iflag=direct status=none | sha256sum" < /dev/null | cut -d' ' -f1)
    echo "NVT_HASH $lba $blocks $h" >> "$OUT/hashes"
  done < "$R"
  wc -l "$OUT/hashes";;
esac
