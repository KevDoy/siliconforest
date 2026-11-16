# Bound ext4 clustered-write allocations at the real EOF

Read-only Linux checks across two clean M1 installer visits found initialized
extents past EOF in Firefox profile files. The new startup-cache case has EOF
52208 and blocks13..15 past EOF. Its logical4KiB blocks complete a16KiB VM page.
No filesystem repair or native fix has been performed by this directory.
Private inode/path/fsck receipts stay under runs/sftb/firefox-install-0044.

The installed ext4 SDK resolves to11f479q… and its frozen source ssrsapdr…;
the mutable upstream checkout is different and is not the installed baseline.
XNU cluster_io explicitly rounds ordinary I/O to PAGE_SIZE, expects BLOCKMAP to
return only the allocation covering real data, and stops once non_rounded_size
is covered. Frozen ext4 instead allocates the complete rounded request. A write
at49152 with EOF52208 therefore allocates blocks12..15 rather than just12.

The narrow eof.patch caps VNODE_WRITE blockmap requests at
max(e_size,e_new_size), including an extending write in progress. The final
filesystem block still covers its valid bytes and padding; a_run lets cluster_io
stop without requesting the unallocated page tail. Reads are unchanged. Existing
bad extents are not freed or repaired. Journal/cache-flush ordering and all NVMe
write/halt fences are unchanged. The documented lock order is mount -> node ->
allocation; the existing node-size helper is called in that order. Inspection of
the frozen node-lock regions finds no inversion across the mount lock.

python3 test-source.py applies the actual patch to exact bytes and compiles the
literal blockmap function and existing EOF helper under ASan/UBSan. An allocation
recorder exposes the frozen13..15 counterexample as an expected assertion failure.
Patched current/growing EOFs, existing extents, read holes, EOF/freed guards and
XNU's literal rounding/tail-stop fragments pass. This is request-length/control
logic with a simulated allocation recorder, NOT a real extent tree, journal,
concurrency, power-loss, filesystem-cleanliness or native runtime proof.

build-ext4.nix freezes the qualified SDK recipe and hashes before/after the one
source file. Native SDK compilation succeeds with96 exported symbols identical to the
qualified driver. Real QEMU filesystem write/truncate/readback/unmount plus
e2fsck rehearsal remains a prerequisite before any M1 install.
No replacement collection or install is scheduled. Linux fsck repairs remain
outside the current bitmap-only permission for these extent cases.

FXPERF also measured4KiB fsync/unlink near14–15ms. That timing is a separate
question: the installed journal already batches at64 metadata blocks/1s except
explicit sync, so a universal per-unlink commit cannot be inferred. Commits do
perform ordered journal and checkpoint flushes; no barrier/durability reduction
is proposed. Measure those stages before selecting a performance change.

Original filesystem/XNU licences remain in their patched sources. These new
build/host-fixture helpers are MIT. Source patches do not redistribute firmware,
private captures or filesystem identifiers.
