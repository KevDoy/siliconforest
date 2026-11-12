#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verdict of a boot that started the NVMe controller read-only.

  nv_verdict.py <linux-before dir> <linux-after dir> <console.bin> <expected write_refused> [level, default 2]

The proof it checks, honest about Linux' own writes:
  - ranges on partitions Linux does not modify (the GPTs and the APFS partitions) must be
    identical in Linux before and after the Darwin boot (nothing changed) and identical to
    what Darwin read (the reads are right);
  - ranges inside Linux' mounted filesystems are compared too, but a difference there is
    only reported: where Linux-before equals Linux-after, Darwin must equal both; where
    Linux itself changed the range around its reboot, nothing can be attributed.
  - Linux afterwards: no recovery/replay lines, its filesystems mounted, unsafe_shutdowns
    unchanged; Darwin: gate_rejected 0, write_refused as expected, stopped cleanly,
    Identify equal to Linux'.
Prints NV_VERDICT lines; exit 0 only when everything holds. A failure writes
runs/m1-storage/STOP so that no further storage boot starts.
"""
import re, subprocess, sys
from pathlib import Path

import os
before, after, console = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
expect_refused = None if sys.argv[4] == 'any' else int(sys.argv[4])
# NV_VERDICT_MODE=part: the SiliconForest partition (first block NV_PART_LBA) was named to the driver and
# mounted read-only; NV_TREE_REF holds the tree digest of the image it was copied from.
part_mode = os.environ.get('NV_VERDICT_MODE') == 'part'
root_mode = os.environ.get('NV_VERDICT_MODE') == 'root'     # the root filesystem was our partition, read-only (guest/darwin-diskroot.in)
ours_lba = int(os.environ.get('NV_PART_LBA', '0'))
level = int(sys.argv[5]) if len(sys.argv) > 5 else 2
bad = []
def check(ok, text):
    print('NV_VERDICT %s %s' % ('ok  ' if ok else 'FAIL', text))
    if not ok: bad.append(text)

def hashes(text, prefix='NVT_HASH'):
    return {(int(a), int(b)): h for a, b, h in re.findall(r'^%s (\d+) (\d+) ([0-9a-f]{64})\s*$' % prefix, text, re.M)}
def parts(d):
    out = []
    for name, start, sectors in re.findall(r'^(nvme0n1p\d+) start=(\d+) sectors=(\d+)', (d / 'sysfs.txt').read_text(), re.M):
        fstype = re.search(r'%s\s+\d+\s+part\s+(\S+)' % name, (d / 'lsblk.txt').read_text())
        out.append((name[7:], int(start) // 8, int(sectors) // 8, fstype.group(1) if fstype else '?'))
    return out
def where(lba, blocks, table):
    for name, start, size, fs in table:
        if lba >= start and lba + blocks <= start + size: return name, fs
        if lba < start + size and lba + blocks > start: return name + '+gap', fs
    return 'gpt/gap', 'none'
def smart(text, key):
    m = re.search(r'%s=(\d+)' % key, text); return int(m.group(1)) if m else None

con = subprocess.run(['strings', '-n', '6', str(console)], capture_output=True, text=True, errors='replace').stdout
hb, ha = hashes((before / 'hashes').read_text()), hashes((after / 'hashes').read_text())
hd = hashes(con)
table = parts(after)
check(len(hd) > 0 and len(ha) >= len(hd), 'hash sets: Linux before %d, Darwin %d, Linux after %d ranges' % (len(hb), len(hd), len(ha)))
static_ok = static_n = stable_n = stable_ok = moved = 0
for (lba, blocks), h in sorted(ha.items()):
    name, fs = where(lba, blocks, table)
    ours = ours_lba and name == next((n for n, st, _, _ in table if st == ours_lba), None)
    quiet = fs in ('apfs', 'none') or bool(ours)    # Linux mounts none of these (ours: nothing may write it in a read-only boot)
    if ours: name = name + '=SF'
    b, d = hb.get((lba, blocks)), hd.get((lba, blocks))
    state = 'unchanged' if b == h else ('no-before' if b is None else 'CHANGED-BY-LINUX-OR-US')
    rd = 'darwin==linux' if d == h else ('darwin-missing' if d is None else 'DARWIN-DIFFERS')
    print('NV_RANGE %10d %4d %-8s %-5s %-22s %s' % (lba, blocks, name, fs, state, rd))
    if quiet:
        static_n += 1; static_ok += (b == h and d == h)
    elif b == h:
        stable_n += 1; stable_ok += (d == h)
    else:
        moved += 1
check(static_n > 0 and static_ok == static_n, 'ranges Linux never writes (GPTs, APFS partitions%s): ' % (', the SiliconForest partition' if ours_lba else '') + '%d of %d unchanged before/after AND equal under Darwin' % (static_ok, static_n))
check(stable_ok == stable_n, 'ranges in Linux-mounted filesystems that Linux left alone: %d of %d equal under Darwin' % (stable_ok, stable_n))
print('NV_VERDICT note %d range(s) in Linux-mounted filesystems differ between Linux before and after (Linux writes there on every boot; not attributable)' % moved)

sb, sa = (before / 'smart-after.txt').read_text(), (after / 'nvme-id.txt').read_text()
for key in ('unsafe_shutdowns', 'media_errors', 'num_err_log_entries', 'critical_warning'):
    kb, ka = smart(sb.replace('0x', ''), key), smart(sa.replace('0x', ''), key)
    check(kb is not None and kb == ka, 'SMART %s: %s before, %s after' % (key, kb, ka))
print('NV_VERDICT note SMART deltas across the cycle: ' + ' '.join('%s=%+d' % (k, smart(sa, k) - smart(sb, k)) for k in
      ('power_cycles', 'data_units_read', 'data_units_written', 'host_read_commands', 'host_write_commands')))
ds = re.findall(r'^NV_SMART (start|end) (.*)$', con, re.M)
if len(ds) == 2:
    print('NV_VERDICT note SMART inside Darwin, start to end: ' + ' '.join('%s=%+d' % (k, smart(ds[1][1], k) - smart(ds[0][1], k)) for k in
          ('power_cycles', 'unsafe_shutdowns', 'data_units_read', 'data_units_written', 'host_read_commands', 'host_write_commands')))
    check(smart(ds[1][1], 'host_write_commands') == smart(ds[0][1], 'host_write_commands') and
          smart(ds[1][1], 'data_units_written') == smart(ds[0][1], 'data_units_written'), 'write counters did not move during the Darwin session')
else:
    check(False, 'SMART start and end lines from Darwin')

klog = (after / 'dmesg.txt').read_text()
recov = [l for l in klog.splitlines() if 'raid6' not in l and (re.search(r'recover|replay|orphan|unclean|not cleanly|corrupt|I/O error', l, re.I) or
         (re.search(r'nvme|sart|BTRFS|EXT4|FAT-fs', l) and re.search(r'error|warn|fail|timeout|crash', l, re.I)))]
check(not recov, 'Linux kernel log after the boot: %d recovery/replay/error line(s)%s' % (len(recov), ''.join('\n    ' + l[:150] for l in recov[:5])))
mounts = (after / 'mounts.txt').read_text()
linux_fs = [('nvme0n1' + n, fs) for n, st, _, fs in table if fs in ('vfat', 'ext4', 'btrfs') and st != ours_lba]
check(len(linux_fs) == 3, 'Linux has its three filesystems (EFI, /boot, root): %s' % linux_fs)
if ours_lba:
    check(not re.search(r'/dev/nvme0n1%s\b' % next((n for n, st, _, _ in table if st == ours_lba), 'pX'), mounts), 'the SiliconForest partition is not mounted in Linux')
for dev, fs in linux_fs:
    check(re.search(r'/dev/%s\S* %s rw' % (dev, fs), mounts) is not None, 'Linux mounted %s (%s) read-write as usual' % (dev, fs))
check('state=live' in (after / 'nvme-sysfs.txt').read_text(), 'Linux NVMe controller state live')

gates = re.findall(r'^NV_GATE_REJECTED (\S+)', con, re.M); refused = re.findall(r'^NV_WRITE_REFUSED (\S+)', con, re.M)
check(bool(gates) and all(g == '0' for g in gates), 'gate_rejected: %s' % ' '.join(gates))
if expect_refused is None:
    check(bool(refused), 'write_refused (requests that reached the driver and were refused there): %s' % ' '.join(refused))
else:
    check(bool(refused) and refused[-1] == str(expect_refused), 'write_refused: %s (expected %d = the deliberate attempts)' % (' '.join(refused), expect_refused))
check(re.search(r'^NV_STATE stopped-clean', con, re.M) is not None, 'controller stopped cleanly inside Darwin')
check(re.search(r'^NV_GATE_SELFTEST allowed=7 refused=505 wrong=0', con, re.M) is not None, 'gate self-test 7/505/0')
check(re.search(r'^NV_NCPU 8', con, re.M) is not None, '8 CPUs')
check('panic(' not in con, 'no panic')
check(not re.search(r'PDAns: REFUSED|TCB invalidation failed|CRASHED|not in flight|no completion|bring-up failed|NOT CLEANLY', con), 'no driver anomaly line')
for tag in ('NV_IDCTRL', 'NV_IDNS'):
    l = re.search(r'^%s (.*)$' % tag, (after / 'nvme-id.txt').read_text(), re.M); d = re.search(r'^%s (.*)$' % tag, con, re.M)
    same = bool(l and d and l.group(1).strip() == d.group(1).strip())
    check(same, '%s fields equal Linux\'' % tag)
    if l and d and not same:
        lf, df = dict(re.findall(r'(\w+)=("[^"]*"|\S+)', l.group(1))), dict(re.findall(r'(\w+)=("[^"]*"|\S+)', d.group(1)))
        print('    differing: ' + ' '.join('%s linux=%s darwin=%s' % (k, lf.get(k), df.get(k)) for k in lf if lf.get(k) != df.get(k)))
if level >= 3 and not root_mode:
    nparts = len(table)
    census = re.findall(r'^NV_MEDIA (.*)$', con, re.M)
    want = 'published=1 partitions=%d guarded=%d foreign=0' % (nparts, nparts)
    if part_mode: want = 'published=1 partitions=%d guarded=%d foreign=0 ours=1' % (nparts, nparts - 1)
    check(len(census) >= 2 and all(c.strip() == want for c in census), 'below the block device: %s (expected "%s", before and after the reads)' % (' | '.join(census), want))
    if part_mode:
        check(not re.search(r'ApfsFileSystemDriver|AppleAPFS|apfs_|hfs_mount|msdosfs_mount', con), 'no APFS, HFS or FAT driver line on the console')
    else:
        check(not re.search(r'ApfsFileSystemDriver|AppleAPFS|apfs_|Ext4FileSystemDriver|hfs_mount|msdosfs_mount|ext4_mount', con), 'no APFS / filesystem driver line on the console')
    mounts_d = re.findall(r'^NV_MOUNT (.*)$', con, re.M)
    check(bool(mounts_d) and not any('/dev/disk' in m for m in mounts_d), 'nothing from a disk is mounted in Darwin (%d mount lines, all root or devfs)' % len(mounts_d))
    hm = hashes(con, 'NVT_MEDIA_HASH')
    okm = sum(1 for k, h in hm.items() if hd.get(k) == h)
    check(len(hm) == len(hd) and okm == len(hm), 'through the block device (/dev/rdiskN): %d of %d ranges equal to the same boot\'s /dev/pdnvme0 reads' % (okm, len(hm)))
    wt = re.findall(r'^NVT_WTEST_RESULT (\S+) (\S+)', con, re.M)
    check(len(wt) == (6 if part_mode else 4) and all(r == 'writes-refused' for _, r in wt), 'write attempts: %s' % ' '.join('%s=%s' % x for x in wt))
    check('GRANTED' not in con, 'no open for writing was granted on any node')
    gpt = re.findall(r'^NV_GPT (\d+) (\d+) (\d+) GPT part', con, re.M)
    check(sorted((int(a), int(b)) for a, b, _ in gpt) == sorted((st, sz) for _, st, sz, _ in table), 'GPT as parsed by the stack: %d partitions with Linux\' starts and sizes' % len(gpt))
if part_mode:
    check(re.search(r'^NV_PART verified first=%d ' % ours_lba, con, re.M) is not None, 'the driver identified our partition in both GPT copies: %s' % ' | '.join(re.findall(r'^NV_PART (.*)$', con, re.M)))
    ours_idx = re.search(r'^NV_OURS index=(\d+)', con, re.M)
    for rnd in ('1', '2'):
        ml = re.findall(r'^NV_MOUNT%s (.*)$' % rnd, con, re.M)
        disk = [m for m in ml if '/dev/disk' in m]
        check(len(disk) == 1 and ours_idx is not None and re.match(r'/dev/disk\d+s%s on /private/tmp/sf \(ext4.*read-only' % ours_idx.group(1), disk[0]) is not None,
              'mount %s: only our partition, ext4, read-only: %s' % (rnd, ' | '.join(disk)))
        check(re.search(r'^NV_UMOUNT%s rc=0' % rnd, con, re.M) is not None, 'unmount %s succeeded' % rnd)
    rw = re.findall(r'^NV_MOUNT_RW_STATE (.*)$', con, re.M)
    check(all('read-only' in m for m in rw), 'a mount without rdonly did not become writable: %s %s' % (' '.join(re.findall(r'^NV_MOUNT_RW (.*)$', con, re.M)), ' | '.join(rw) or '(not mounted)'))
    fw = re.findall(r'^NV_FSWRITE (\S+) rc=(\d+) ?(.*)$', con, re.M)
    check(len(fw) == 5 and all(rc != '0' for _, rc, _ in fw), 'writes into the mounted filesystem refused: %s' % '; '.join('%s rc=%s %s' % x for x in fw))
    up = re.findall(r'^NV_MOUNT_AFTER_UPGRADE (.*)$', con, re.M)
    check(len(up) == 1 and 'read-only' in up[0], 'still read-only after the attempt to remount read-write')
    ref = Path(os.environ['NV_TREE_REF']).read_text()
    def tree(text, tag='NVT_TREE_ALL'):
        m = re.search(r'^%s (files=\d+ dirs=\d+ links=\d+ other=\d+ bytes=\d+ errors=\d+ [0-9a-f]{64})' % tag, text, re.M); return m.group(1) if m else None
    check(tree(con) is not None and tree(con) == tree(ref) and 'errors=0' in tree(con), 'the whole tree read from the mounted partition equals the source image: %s' % tree(con))
    check(tree(con, 'NVT_TREE2_ALL') == tree(ref), 'the same digest from the second mount')
    tops_d, tops_r = dict(re.findall(r'^NVT_TREE_TOP (\S+) ([0-9a-f]{64})', con, re.M)), dict(re.findall(r'^NVT_TREE_TOP (\S+) ([0-9a-f]{64})', ref, re.M))
    if tops_d != tops_r: print('    top-level entries that differ: ' + ' '.join(sorted(k for k in set(tops_d) | set(tops_r) if tops_d.get(k) != tops_r.get(k))))
    files_d, files_r = re.findall(r'^NVT_FILE (\S+ \d+ [0-9a-f]{64})', con, re.M), re.findall(r'^NVT_FILE (\S+ \d+ [0-9a-f]{64})', ref, re.M)
    check(len(files_d) == 3 and files_d == files_r, 'three named files hash as in the source image: %s' % ' '.join(f.split()[0] for f in files_d))
    am = re.search(r'^NVT_AFTER_MOUNT_HASH_ALL ranges=(\d+) errors=0 ([0-9a-f]{64})', con, re.M); mm = re.search(r'^NVT_MEDIA_HASH_ALL ranges=(\d+) errors=0 ([0-9a-f]{64})', con, re.M)
    check(bool(am and mm and am.group(2) == mm.group(2)), 'all ranges hash the same after the mounts as before them (inside Darwin)')
    check(not re.search(r'^NV_FAIL', con, re.M), 'no NV_FAIL line')
    for l in re.findall(r'^NV_DMESG .*$|^.*ext4: .*$', con, re.M)[:40]: print('NV_RESULT ' + l[:200])
if root_mode:
    nparts = len(table)
    def tree(text, tag='NVT_TREE_ALL'):
        m = re.search(r'^%s (files=\d+ dirs=\d+ links=\d+ other=\d+ bytes=\d+ errors=\d+ [0-9a-f]{64})' % tag, text, re.M); return m.group(1) if m else None
    ref = Path(os.environ['NV_TREE_REF']).read_text()
    check(re.search(r'^NV_ROOT disk read-only: root_device on / \(ext4.*read-only.*\) on disk\d+s\d+', con, re.M) is not None, 'the root filesystem is our partition, ext4, read-only: %s' % ' | '.join(re.findall(r'^NV_ROOT (.*)$', con, re.M)))
    check(re.search(r'^NV_BOOTARGS .*rd=uuid boot-uuid=GUID', con, re.M) is not None and not re.search(r'^NV_BOOTARGS .*rd=md0', con, re.M), 'booted with rd=uuid boot-uuid=<partition GUID>, no rd=md0')
    check(re.search(r'^NV_MOUNT /dev/md0 on /private/ram \(ext4(?!.*read-only)', con, re.M) is not None, 'the memory disk is mounted writable on /private/ram')
    check('sf-roinit: root read-only' in con, 'process 1 was the wrapper (sf-roinit line on the console)')
    check(re.search(r'^NV_PART verified first=%d ' % ours_lba, con, re.M) is not None, 'the driver identified our partition in both GPT copies')
    census = re.findall(r'^NV_(?:MEDIA|BEFORE_STOP media|AFTER_STOP media) (.*)$', con, re.M)
    want = 'published=1 partitions=%d guarded=%d foreign=0 ours=1' % (nparts, nparts - 1)
    check(len(census) >= 3 and all(c.strip() == want for c in census), 'below the block device, %d readings: %s' % (len(census), ' | '.join(sorted(set(census)))))
    fences = re.findall(r'^NV_(?:FENCE|BEFORE_STOP fence|AFTER_STOP fence) (.*)$', con, re.M)
    f = lambda t, k: int(re.search(r'\b%s=(\d+)' % k, t).group(1))
    check(len(fences) >= 4 and all(f(t, 'readfence') == 1 and f(t, 'other') == 0 for t in fences), 'read fence on; requests served outside the GPT and our partition: 0 in every reading')
    check(len(fences) >= 4 and f(fences[0], 'refused') == 0, 'before the deliberate test NOTHING had asked for a block of an owner partition through the block device (refused=0): %s' % (fences[0] if fences else ''))
    check(len(fences) >= 4 and 1 <= f(fences[-1], 'refused') <= 8 and 'NVT_FAIL' in ''.join(re.findall(r'^NV_FENCETEST (.*)$', con, re.M)), 'the deliberate read of an owner partition through /dev/rdiskN was refused: %s' % (fences[-1] if fences else ''))
    print('NV_VERDICT note reads of our partition through the block device: %s' % (fences[-1] if fences else ''))
    sw = re.findall(r'^NV_STATEWRITE (\S+) rc=(\d+)', con, re.M)
    check(len(sw) == 4 and all(rc == '0' for _, rc in sw), 'writes to /var, /tmp, /private/var/tmp, /etc/resolv.conf succeed (memory): %s' % ' '.join('%s=%s' % x for x in sw))
    fw = re.findall(r'^NV_FSWRITE (\S+) rc=(\d+) ?(.*)$', con, re.M)
    check(len(fw) == 5 and all(rc != '0' for _, rc, _ in fw), 'writes to the root filesystem refused: %s' % '; '.join('%s rc=%s %s' % x for x in fw))
    up = re.findall(r'^NV_MOUNT_AFTER_UPGRADE (.*)$', con, re.M)
    check(len(up) == 1 and 'read-only' in up[0], 'root still read-only after the attempt to remount read-write')
    check(tree(con) is not None and tree(con) == tree(ref) and 'errors=0' in tree(con), 'the whole root tree as the running system sees it equals the transformed source image: %s' % tree(con))
    files_d, files_r = re.findall(r'^NVT_FILE (\S+ \d+ [0-9a-f]{64})', con, re.M), re.findall(r'^NVT_FILE (\S+ \d+ [0-9a-f]{64})', ref, re.M)
    # the libSystem in use reads back changed in the cache (the same other digest in QEMU and on the M1, read-only
    # or writable root); everything else must be exact
    IN_USE = 'usr/lib/libSystem.B.dylib 4338830 83e40e0bcd4452917ffcc01815a16b41753fe4e478cf7e5ba092ef4c00fba765'
    check(len(files_d) == 3 and files_d[:2] == files_r[:2] and files_d[2] in (files_r[2], IN_USE), 'named files hash as in the source image (libSystem: as on disk, or the known in-use digest): %s' % ('in-use digest' if files_d[2:] == [IN_USE] else 'as on disk'))
    for l in re.findall(r'^NV_LIBSYSTEM .*$', con, re.M): print('NV_RESULT ' + l[:220])
    check(not re.search(r'^NV_FAIL', con, re.M), 'no NV_FAIL line')
    check('PD_BENCH_DONE' in con.split('NV_AFTER_STOP')[-1], 'the report after the stop came out complete (from memory)')
    print('NV_VERDICT note writes the ext4 driver tried and IOMedia refused ("privilege violation" lines): %d' % con.count('privilege violation'))
    for l in re.findall(r'^NV_DMESG .*$', con, re.M)[:30]: print('NV_RESULT ' + l[:200])
raws = dict(re.findall(r'^NV_SMARTRAW (\S+) ([0-9a-f]{1024})', con, re.M))
order = [k for k in ('start', 'end', 'start2', 'end2') if k in raws]
for a, b in zip(order, order[1:]):
    x, y = bytes.fromhex(raws[a]), bytes.fromhex(raws[b])
    diff = [i for i in range(512) if x[i] != y[i]]
    fields = sorted({(i // 16) * 16 if i >= 32 else i for i in diff})
    print('NV_VERDICT note SMART page %s -> %s: bytes differing at offsets %s (1-2 temperature, 32 data units read, 64 host reads, 96 busy time, 112 power cycles, 144 unsafe shutdowns, 48/80 writes; >=192 vendor/other)' % (a, b, fields))
    check(not any(f in (48, 80, 144, 160, 176) or f == 0 for f in fields), 'SMART %s -> %s: no change in write counters, unsafe shutdowns, media errors, error log, critical warning' % (a, b))
starts = con.split('PDAppleNVMe: start ')
for chunk in starts[1:]:
    n = chunk.split(' ', 1)[0]
    msgs = re.findall(r'PDAns: syslog: (.*)', chunk)
    print('NV_VERDICT note firmware log after start %s: %s' % (n, ' || '.join(m.strip()[:90] for m in msgs) or '(none)'))
if 'start2' in raws:
    check(len(re.findall(r'^NV_STATE stopped-clean', con, re.M)) >= 2 and 'end2' in raws, 'second start in the same boot: ready, read, stopped cleanly again')
    hs = re.search(r'^NVT_SECOND_HASH_ALL ranges=(\d+) errors=0 ([0-9a-f]{64})', con, re.M); h1 = re.search(r'^NVT_HASH_ALL ranges=(\d+) errors=0 ([0-9a-f]{64})', con, re.M)
    check(bool(hs and h1 and hs.group(2) == h1.group(2)), 'second start: all ranges hash to the same digest as on the first start')
for l in re.findall(r'^NVT_(?:MEDIA_)?(?:SEQ|RAND) .*$|^NVT_WTEST.*$|^NVT_FAIL.*$|^NV_FAIL.*$', con, re.M): print('NV_RESULT ' + l)
if bad:
    stop = Path(__file__).resolve().parents[4] / 'runs/m1-storage/STOP'
    stop.write_text('nv_verdict failed for %s: %s\n' % (console, '; '.join(bad)))
    print('NV_VERDICT FAILED (%d); %s written: no further storage boot' % (len(bad), stop)); sys.exit(1)
print('NV_VERDICT PASSED')
