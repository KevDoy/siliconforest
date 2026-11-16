# Read pending ext4 journal metadata before issuing a disk read

Kernel 47's small-append latency is variable under the shared desktop. In one
native batch of 300 100-byte writes, every call completed below 0.7 ms; a repeat
had 98 calls above 2 ms, with a 12.3 ms p90 and 29 ms maximum. A preallocated
overwrite control also had stalls. Slow calls consumed tens to hundreds of
microseconds of thread CPU, so these were predominantly waits, not CPU work.
The background was active; these are not controlled performance A/B results.

A bounded 16-stack capture found a concrete wait edge: our append thread was
waiting for ext4's mount mutex, owned by the compositor's thread during
close/inactive/free-inode-extents. The owner was in
`ext4_write_super -> ext4_blkread -> buf_meta_bread -> PDAppleNVMeRW pd_io`,
waiting for synchronous storage I/O. Another snapshot caught our writer doing
synchronous block zero-initialization. Not every stall has been attributed.
This does not establish a journal commit on every write. Existing metadata
writes cache their journal image, while commits batch at 64 blocks or one
second and perform two ordered flushes. The native fsync/unlink sample had
medians about 10.8/11.3 ms. Those calls need their own stage attribution.

One avoidable operation is source-concrete: `ext4_blkread` first reads the
physical metadata block, then overlays its newer pending journal image.
If the authoritative full block is already in the journal transaction, the
disk read is unnecessary. The candidate checks that image first using a
borrowed metadata buffer. On a hit it fills the entire buffer, clears its
error/residual and marks it invalid for release. It issues no synthetic I/O
completion, delayed write, or physical write. On a miss, disabled cache,
absent journal or failed buffer acquisition, the existing read/overlay path
remains. Callers retain the metadata mount lock; bootstrap is single-threaded.
The invalid borrowed buffer is discarded on release, leaving the transaction
image authoritative until the existing checkpoint writes it.

`ext4_subr.c` contains the reader and counters; `ext4_vfsops.c` explicitly
registers/unregisters their sysctls with successful filesystem registration.
The write allocator, EOF fix, journal log/commit,
checkpoint order, both device flushes, NVMe driver and frozen write-gate/fence
files remain unchanged. `kern.pd_ext4.journal_cache` permits runtime 0/1 A/B;
`kern.pd_ext4.cache_stats` exposes aggregate reads, journal hits and physical
bread calls. Counters are atomic; no timer or polling worker is added.

The exact deployed 45/46/47 SDK baseline is asserted. The full SDK build
succeeds. The actual patched reader and unchanged journal overlay compile and
pass ASan/UBSan with a fake I/O provider: exact data/sector units, hit despite
physical read failure, invalid clean buffer, disabled/no-journal/miss,
late-overlay, I/O error and balanced buffer ownership. These are control and
failure tests, not real buffer-cache, journal replay, concurrency or crash
consistency proof. Disposable QEMU validation precedes any M1 install.

The isolated QEMU guest tests both runtime settings, 32 create/read/chmod/
rename/copy/unlink cases each, the existing 28 write/mmap plus seven resize
cases, full independent data oracles and a read-only filesystem check after
clean unmount. Cache counters must show real hits and unchanged hit counts
while disabled. A first controller failed before boot on an already-used
image output name; a second VM booted but failed when the new sysctls were
not registered. Both failures are preserved. Explicit lifecycle registration
was added before a fresh VM rehearsal. The corrected VM passes: disabled hits
delta 0, enabled hits delta 6272, all 64 metadata payloads and the 29 existing
write/resize payloads match independent debugfs byte oracles, USB 8 MiB reads
match and the cleanly unmounted scratch filesystem passes e2fsck -fn with
status 0. The candidate collection has 57 modules, only ext4 changes from
qualified 47, with the core and 56 other UUIDs identical. Native qualification
is pending; this candidate is not installed and no kernel 48 slot is reserved.
No append speed improvement or general filesystem repair is claimed.

Private evidence: `runs/sftb/dmabuf-fences/` (native baseline/stacks) and
`runs/sftb/ext4-journal-cache/` (host/SDK/VM candidate). The initial stack capture
failed because the Python ctypes variadic syscall signature was undeclared on
arm64; the corrected attempt recorded 0.38–0.68 ms per snapshot. Native stack
buffers and device identifiers remain private.
