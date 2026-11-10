# splits `diff -ruN a b` of the scratch layer into the topic patches under lkpi/patches-layer
import re, subprocess, sys, os
out = sys.argv[1]
# the directory holding a/ (pristine tree slice) and b/ (edited), see regen.sh
work = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))
d = subprocess.run(["diff", "-ruN", "a", "b"], capture_output=True, text=True, cwd=work).stdout
files = re.split(r"(?m)^diff -ruN .*\n", d)[1:]
topics = {
 "0001-page-size-follows-the-kernel-16k": ("""fbsdkpi: the layer's page size is the kernel's (16 KiB on Apple Silicon)

LinuxKPI and the FreeBSD compatibility code were built for 4 KiB pages on every arm64 board,
while the xnu seam (pd_fbsd_os.cpp: pdos_map_pages, pdos_malloc_contig, pdos_vtophys) already
works in xnu pages. PAGESIZE/B_PAGE_SIZE/PAGE_SIZE and PAGE_SHIFT are the only two places
the number is written down; everything else in the layer derives from them.
UPSTREAM: yes, as a per-board setting (boards with 4 KiB kernels keep 4096/12).
""", []),
 "0002-xnu-seam-nonblocking-malloc-and-lock-storage": ("""fbsdkpi: xnu seam: non-blocking pdos_malloc, locks in caller storage, counters

pdos_malloc ignored `canwait` (IOMalloc always may block). It now uses kalloc_data, the kpi
that takes Z_NOWAIT. pdos_mtx_setup/pdos_rw_setup make an unlocked xnu lock in 16 bytes of
caller storage, so the Haiku locks above no longer allocate (next patch). Counters for tests.
UPSTREAM: yes.
""", []),
 "0003-haiku-locks-embed-the-xnu-lock": ("""fbsdkpi: Haiku mutex and rw_lock hold their xnu lock instead of allocating it

The xnu lock behind a mutex/rw_lock (and so behind every FreeBSD mtx/sx and LinuxKPI mutex,
spinlock_t and wait queue) was allocated on first use and freed only by *_destroy. Linux code
never has to destroy a lock (mutex_destroy is a debug no-op there, spinlocks have none), so
every lock that was dropped with its container leaked one xnu lock: the Rust crate's Mutex,
SpinLock and CondVar, and equally the DRM C core. The lock is now two words inside the Haiku
lock; nothing is allocated and nothing needs freeing.
UPSTREAM: yes (fixes the leak for all LinuxKPI/DRM kexts).
""", []),
 "0005-driver-core-surface-of-linux-7.2": ("""fbsdkpi: the driver-core surface Linux 7.2 drivers (and the Rust abstractions) use

- struct device_driver: acpi_match_table, suppress_bind_attrs, p_cb.post_unbind_rust
- struct device_node embeds its fwnode_handle; linux/property.h and linux/of_reserved_mem.h
  with the prototypes of pd_linux_fwnode.c (the file itself is m1-gpu-dt's, added to the
  LinuxKPI source list here)
- pd_linux_drvcore.c: devres_node_* (7.2's managed-resource nodes, carried by LinuxKPI's own
  devres list), __platform_driver_register, platform_device_put,
  platform_get_irq_byname_optional, platform_bus_type, acpi_of_match_device (no ACPI: false),
  of_root and friends set at init
UPSTREAM: yes, with m1-gpu-dt's pd_linux_fwnode.c and pd_linux_of.patch.
""", []),
 "0006-of-text-properties-named-by-the-node": ("""fbsdkpi: of: a node can say which of its properties are text (pd,string-props)

m1-gpu-dt's kernel/pd_linux_of.patch, change 1, re-made against the file because that patch
does not apply as written (hunk line counts). to_fdt() guesses text from the bytes; one value
of the M1's GPU table (opp01 opp-microwatt = 0x003a4148, stored 48 41 3a 00) passes for a
string and would reach the driver unswapped. PDGpuDT lists its text properties.
One addition to that patch, found on hardware (boot rp01: opp1 still read 1212234240):
PDGpuDT sets the list only on nodes that have text, and the opp nodes have none. A node
without the list below a node with one is taken to have no text at all.
UPSTREAM: yes.
""", []),
 "0008-platform-pre-probe-hook": ("""fbsdkpi: platform bus: a hook between making the device and probing it

pd_platform_pre_probe, when a kext sets it, is called with the platform_device right before
the driver's probe; a nonzero return keeps the device from being probed. PDAgx uses it to name the GPU's mailbox (pd_apple_mbox_bind): Apple's
device tree has no mboxes phandle from the GPU to gfx-asc, and the driver is unmodified.
UPSTREAM: only until the published GPU node carries a mailbox reference the layer resolves.

Also in this file (boot dp04): a device is entered in the bus's device table before its probe
runs, as Linux registers a device first, so that a probe, or a component bind running
inside it, finds its own device with of_find_device_by_node(); the entry is removed when the
pre-probe hook or the probe fails. UPSTREAM for this part: yes.
""", []),
 "0009-physmem-is-the-machines": ("""fbsdkpi: si_meminfo() reports the machine's memory, not a constant 4 GiB

pd_lkpi_physmem() was a constant. It now asks the xnu seam (pdos_physmem(): hw.memsize; that
function is in patch 0002's files). Found by m1-lkpi-extras: the Asahi driver sizes its GPU
memory limits from totalram.
UPSTREAM: yes.
""", []),
 "0010-dma-map-sg-for-platform-devices": ("""fbsdkpi: dma_map_sg works for a device-tree platform device, and its failure is an error

- bus_dma: a tag without an IOKit device (bus_get_dma_tag() only knows pci, so every
  platform device) made _bus_dmamap_load_phys() return EINVAL, and so dma_map_sg map nothing.
  Such a device has no IOMapper; the seam's identity mapping applies.
- dma_map_sgtable() returned 0 with nents 0 when nothing was mapped. Linux returns an error,
  and a caller that trusts 0 (drm_gem_shmem_get_pages_sgt) went on with an empty table.
UPSTREAM: yes.
""", []),
 "0011-page-pool-for-single-pages": ("""fbsdkpi: single pages come from a pool of 1 MiB wired chunks; M_NOWAIT waits when it can

Boot ag03: alloc_page() failed after 66 pages (and the Asahi driver's third 1 MiB GEM object
with it) on a machine with 16 GiB. Each page was one physically-contiguous
IOBufferMemoryDescriptor plus a record allocated M_NOWAIT, and since patch 0002 a M_NOWAIT
allocation really is Z_NOWAIT, which fails whenever the kalloc zone has to grow.
- pd_linux_page.c: order-0 pages without GFP_DMA32 are taken from chunks of 64 pages
  (pdos_pages_alloc: one ordinary wired xnu allocation, per-page physical addresses, no
  contiguity or mask), with a free bitmap per chunk; a fully free, unmapped chunk goes back
  to xnu above a 4 MiB reserve. Higher orders and GFP_DMA32 are unchanged. The page hash
  grows to 16384 buckets. pd_linux_page_stats() for diagnostics.
- vmap() of pool pages maps sub-ranges of the buffer descriptors that own them
  (pdos_map_owned_pages) instead of a physical-address descriptor over xnu-managed pages
  (boot ag05: "IOMemoryDescriptor physical with managed page" on a DEBUG kernel); the
  physical path stays as the fallback and for pages that are not from the pool.
- xnu seam (in patch 0002's files): pdos_pages_alloc/free; pdos_malloc(canwait=0) retries
  blocking when the calling thread may block (not interrupt context, interrupts and
  preemption enabled), counted by pdos_malloc_nowait_retried().
UPSTREAM: yes.
""", []),
 "0012-dri-ioctl-restart-codes-are-eintr": ("""fbsdkpi: drm: an ioctl interrupted by a signal returns EINTR, not Linux's 512

pd_dri_ioctl() handed -ERESTARTSYS (and its relatives) to user space as errno 512. Linux
never shows those to a process. EINTR is what libdrm's drmIoctl() retries on.
UPSTREAM: yes.
""", []),
 "0013-layer-tables-off-the-image": ("""fbsdkpi: the thread and area tables are allocated at load instead of being .bss

sThreads (8192 slots, 720 KiB) and sAreas (4096 slots, 128 KiB) were static arrays in every
kext that carries the layer; the kernel collection has 16 MiB for all kexts together. They
are now allocated by a static constructor when the kext loads. Same sizes, same behaviour.
(The page hash of patch 0011 is allocated the same way, in lkpi_page_init.)
UPSTREAM: yes (saves 850 KiB of collection space per LinuxKPI kext).
""", []),
 "0014-thread-slots-follow-the-xnu-thread": ("""fbsdkpi: a thread slot is dropped when its thread_t is reused by another thread

The layer keeps per-thread state (linuxkpi's task_struct among it) in a table keyed by the
thread_t address, and is never told when a thread it did not start exits. A new thread at a
recycled address inherited the dead thread's state. Each slot now records xnu's unique thread
id (thread_tid); on a mismatch the old state is released and the slot starts clean. The
table therefore holds at most one slot per thread_t address ever seen. pd_thread_slot_stats()
for diagnostics. (pdos_thread_tid() is in patch 0002's files.)
UPSTREAM: yes.
""", []),
 "0015-memory-pressure-runs-the-shrinkers": ("""fbsdkpi: xnu memory pressure runs the Linux shrinkers and trims the page pool

One kernel thread per kext waits in mach_vm_pressure_monitor() (com.apple.kpi.mach) and, at
most once a second while the pageout daemon wants pages, calls linuxkpi's shrinkers
(pd_lkpi_lowmem(), the entry FreeBSD's vm_lowmem event would have used) and then returns
every free pool chunk to xnu (pd_linux_page_trim()). The seam half (pdos_pressure_start) is
in patch 0002's files, the pool half in 0011's.
UPSTREAM: yes.
""", []),
 "0016-scoped-guard-labels-are-unique": ("""linuxkpi: two scoped_guard() in one function no longer define the same label

linux/cleanup.h built the label as `___label_ ## __COUNTER__`; the paste keeps __COUNTER__
from expanding, so every scoped_guard() used the label ___label___COUNTER__ and a function
with two of them did not compile (drivers/gpu/drm/apple has 16 such functions).
UPSTREAM: yes (FreeBSD LinuxKPI).
""", []),
 "0017-backlight-and-platform-driver-fields-of-linux-7": ("""linuxkpi: backlight and platform_driver fields a Linux 7 display driver names

- linux/backlight.h: BACKLIGHT_PLATFORM/FIRMWARE, enum backlight_scale,
  backlight_properties.state/.scale, BL_CORE_SUSPENDED/FBBLANK, backlight_ops.controls_device
- platform_driver.driver_managed_dma
Declarations only: nothing in the layer acts on them yet.
UPSTREAM: yes.
""", []),
 "0018-of-find-compatible-node-continues-after-from": ("""fbsdkpi: of_find_compatible_node() continues after `from`

It ignored `from` and returned the first match every time: for_each_compatible_node() and
anything built on it looped forever once a node matched (boot dp01, the display driver's
for_each_matching_node over "apple,dcp"). Now a pre-order walk from the tree root that
returns the first match after `from`. Found and written by m1-dcp-dart (ANSWERS.md, item 3).
UPSTREAM: yes.
""", []),
 "0019-dma-buf-of-another-kext-is-a-dma-buf": ("""fbsdkpi: drm: a dma-buf file made by another DRM kext is recognised

is_dma_buf_file() compared the file's operations with this kext's own table. Every DRM kext
links its own copy of pd_dma_buf.c, so a dma-buf exported by one (PDAgx's render node) was
not a dma-buf to another (PDDcp's card node): dma_buf_get() returned -EINVAL. linux_file gets
a kind word, set for dma-buf files; both copies are built from the same sources, so the
structures match.
UPSTREAM: yes.
""", []),
 "0020-dri-fds-wake-select-poll-kqueue": ("""pd_dri: select(), poll() and kqueue on a DRM fd wake when an event is queued

dri_select() only reported readiness and never recorded the waiter: select() slept until
its timeout, and poll()/kqueue refused the device (xnu implements poll with kqueue, and a
character device must opt in with cdevsw_setkqueueok()).
- PDDRMDri: a selinfo per open file; dri_select() records the waiter and re-checks;
  pd_dri_wakeup(cookie) wakes it; the major opts in to kqueue.
- fbsdkpi/drm/src/pd_drm_cdev.c: each open DRM file keeps an entry on its drm_file's event
  wait queue that forwards the DRM core's wake_up to pd_dri_wakeup().
UPSTREAM: yes (PDDRMDri and fbsdkpi together).
""", []),
 "0021-dma-buf-sync-file-ioctls": ("""fbsdkpi: drm: DMA_BUF_IOCTL_EXPORT_SYNC_FILE and IMPORT_SYNC_FILE

Both returned ENOTTY. Mesa calls them for every shared buffer on every submission and
falls back to waiting on the CPU when they fail. dma_buf_export_sync_file() and
dma_buf_import_sync_file() of the pin's drivers/dma-buf/dma-buf.c (lines 450-551), compared
line by line, on the layer's dma_resv, dma_fence_unwrap and sync_file.
UPSTREAM: yes.
""", []),
 "0004-allocator-exact-sizes-alignment-realloc": ("""fbsdkpi: allocator: exact ksize, malloc_aligned, realloc zeroing, contigmalloc fixes

- heap blocks record the size that was asked for; malloc_usable_size()/ksize() return it
  (they returned the padded size, so krealloc could hand out stale padding as data)
- ksize()/realloc on contigmalloc memory (every kmalloc above a page) read the word in front
  of the area as a heap header; they now ask the area table, and realloc frees with
  _kernel_free
- realloc with M_ZERO (__GFP_ZERO) zeroes the bytes past the old contents
- malloc_aligned() honours its alignment (it was plain malloc)
- kmalloc above a page with GFP_ATOMIC/GFP_NOWAIT fails instead of blocking in contigmalloc
UPSTREAM: yes.
""", []),
}
def topic(path, hunk, lineno):
    if path.endswith("pd_linux_page.c") and re.search(r"pd_pool|pooled|PD_PAGE_HASH|pd_page_fail|pdos_pages|pdos_map_owned|pd_page_hash = |\*\*pd_page_hash|pressure|pd_linux_page_trim", hunk): return "0011-page-pool-for-single-pages"
    if path.endswith(("limits.h", "param.h", "pd_linux_page.c")): return "0001-page-size-follows-the-kernel-16k"
    if path.endswith(("pd_fbsd_os.h", "pd_fbsd_os.cpp")): return "0002-xnu-seam-nonblocking-malloc-and-lock-storage"
    if path.endswith(("linux/device.h", "linux/of.h", "platform_device.h", "fbsdkpi.cmake", "property.h", "of_reserved_mem.h", "pd_drvcore.h", "pd_linux_drvcore.c")): return "0005-driver-core-surface-of-linux-7.2"
    if path.endswith("pd_lkpi_kern.c"): return "0009-physmem-is-the-machines"
    if path.endswith(("bus_dma.cpp", "dma-mapping.h")): return "0010-dma-map-sg-for-platform-devices"
    if path.endswith(("PDDRMDri.cpp", "pd_dri_cdev.h")): return "0020-dri-fds-wake-select-poll-kqueue"
    if path.endswith("pd_drm_cdev.c") and re.search(r"pd_dri_event_wake|event_wait|wait_queue_t wake|drm_file.h", hunk): return "0020-dri-fds-wake-select-poll-kqueue"
    if path.endswith("pd_drm_cdev.c"): return "0012-dri-ioctl-restart-codes-are-eintr"
    if path.endswith("linux/backlight.h") or (path.endswith("platform_device.h") and "driver_managed_dma" in hunk): return "0017-backlight-and-platform-driver-fields-of-linux-7"
    if path.endswith("pd_dma_buf.c") and re.search(r"sync_file|SYNC_FILE|dma-resv.h|dma-fence", hunk): return "0021-dma-buf-sync-file-ioctls"
    if path.endswith(("linux/fs.h", "pd_dma_buf.c")): return "0019-dma-buf-of-another-kext-is-a-dma-buf"
    if path.endswith("linux/cleanup.h"): return "0016-scoped-guard-labels-are-unique"
    if path.endswith("pd_linux_platform.c"): return "0008-platform-pre-probe-hook"
    if path.endswith("pd_linux_of.c") and "pd_of_next" in hunk: return "0018-of-find-compatible-node-continues-after-from"
    if path.endswith("pd_linux_of.c"): return "0006-of-text-properties-named-by-the-node"
    if path.endswith("lock.h"): return "0003-haiku-locks-embed-the-xnu-lock"
    if path.endswith("pd_haiku_kernel.cpp") and re.search(r"pd_tid|sThreadsRecycled|stale", hunk): return "0014-thread-slots-follow-the-xnu-thread"
    if path.endswith("linux_shrinker.c"): return "0015-memory-pressure-runs-the-shrinkers"
    if path.endswith("pd_haiku_kernel.cpp") and re.search(r"pd_tables", hunk): return "0013-layer-tables-off-the-image"
    if path.endswith("pd_haiku_kernel.cpp"):
        return "0004-allocator-exact-sizes-alignment-realloc" if re.search(r"heap|malloc", hunk) else "0003-haiku-locks-embed-the-xnu-lock"
    return "0004-allocator-exact-sizes-alignment-realloc"
for f in files:
    m = re.match(r"--- (a/\S+).*\n\+\+\+ (b/\S+).*\n", f)
    path = m.group(1)[2:]
    hunks = re.split(r"(?m)^(?=@@ )", f[m.end():])
    per = {}
    for h in hunks:
        if not h.strip(): continue
        per.setdefault(topic(path, h, 0), []).append(h)
    for t, hs in per.items():
        topics[t][1].append(f"--- a/{path}\n+++ b/{path}\n" + "".join(hs))
for t, (msg, parts) in topics.items():
    text = msg + "\n" + "".join(parts)
    open(f"{out}/{t}.patch", "w").write(text)
    print(t, "+%d -%d" % (len(re.findall(r"(?m)^\+[^+]", text)), len(re.findall(r"(?m)^-[^-]", text))))
