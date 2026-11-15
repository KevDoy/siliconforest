# Decoder-to-GPU contract for one future kernel installation

The current installed AVD API allocates private capture pages and maps them
read-only to the decoder process. It does not export a GPU-importable dma-buf.
The current GPU ABI allocates/imports GEM BOs: VM_BIND.userptr names an array
of bind operations, not arbitrary decoder image pages. Consequently an extra
GL upload cannot honestly be called zero CPU copy without a real page-import
path. Existing one-copy GPU-backed NV12 and CGL blits remain the fallback.

Choose the already-staged **caller-owned GPU BO import into AVD**, preserving
ABI1 and advertising PDAVD_CAP_EXTERNAL_BUFFERS bit16 only for implemented
support. No second decoder buffer-export ABI or competing IOSurface/CGL
provider is requested. FX's existing BO/fileport/broker keeps the same object
available in the RDD and GPU process.

- Import the entire capture set before MAP/HOLD/decode, owning task and PID
  checked. Validate writable16KiB-aligned ranges, count, overflow, virtual and
  physical aliases, stride, UV offset and all driver/private-tail capacity.
- Data begins at BO base+16384. Exclude the shared atomic header page. Use
  NV12 420v/f, visible dimensions, Y0, driver UV offset/stride and rounded full
  sizeimage. Reject unsupported layouts and10-bit rather than relabeling them.
- Pin/map every descriptor before an allocation-free all-or-nothing commit.
  MD/kernel-map/task/GEM pager ownership must survive caller unmap/process
  death while AVD can still touch the pages. A failed DART invalidation or
  ambiguous decode completion quarantines pages/IOVAs/pins until reboot.
- Establish real CPU↔AVD↔GPU cache ownership for WB data pages. Pinning,
  prepare/complete, DART invalidation, barriers and no-op cache KPIs do not
  establish visibility. Keep the atomic header WB and avoid conflicting aliases.
- Include the8-line IRQ attribution fix41b6f2d34, preserving its actual-source
  race tests. It is not a proved hardware fix for the rare wrong frame.

TB's exact frozen-kernel audit shows ordinary flush APIs are barrier-only;
CleanPoC_DcacheRegion_Force performs actual clean/invalidate and is a candidate.
Its proposed isolated patch uses bounded page calls at initialization,
before device ownership, after real completion before CPU chroma handling and
after CPU writes before publication. **Clean-after-device-write requires no
stale CPU-dirty lines:** exclude CPU writers/readers during device ownership,
otherwise writeback can overwrite fresh decoded bytes. Cache correctness and
actual exported-BO UPL/pager retention remain hardware-unqualified.

Producer retains AVFrame/CV/IOSurface storage; consumer acquires before producer
release and holds through actual GPU completion. IsInUse/count zero is not a
GPU fence. Process-death quarantine is safe but not automatic cleanup parity.
The userland lease/IPC/fence chain must be proved with FX independently of the
kernel's pinning. No frame becomes recyclable merely because a callback returns.

Exact source/artifact, cache-order and frozen-base review is in
`runs/m1-coord/requests-for-CODEX-TB.md`. TB owns the one combined future kernel
with K14 on known-good NON-AVE29, excluding failed dock diagnostics/DP changes.
Root does not build/install/boot a kernel. The owner must open the window;
source/host/SDK results do not authorize a trial.

Qualification requires independent full-frame hashes/PTS, actual GPU FD/backing,
held64/pool-pressure/flush/process-death controls, matching AVD completions and
no new faults/stale events. Then measure real browser1080p60 and4K30: zero copied
pixel bytes, copies per frame, separate RDD/family CPU, dropped frames and rVFC
cadence. Existing125–140% CPU and40ms callbacks are private one-copy baselines,
not proof of the cost's location or installed-owner performance.
