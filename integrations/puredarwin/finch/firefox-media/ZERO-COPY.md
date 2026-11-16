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
Root does not build/install/boot a kernel. The durable owner grant now authorizes Firefox kernel optimizations without
another decision. TB announces and owns the next idle installation slot;
source/host/SDK results remain separate from native qualification.

Qualification requires independent full-frame hashes/PTS, actual GPU FD/backing,
held64/pool-pressure/flush/process-death controls, matching AVD completions and
no new faults/stale events. Then measure real browser1080p60 and4K30: zero copied
pixel bytes, copies per frame, separate RDD/family CPU, dropped frames and rVFC
cadence. Existing125–140% CPU and40ms callbacks are private one-copy baselines,
not proof of the cost's location or installed-owner performance.

## Consumer source audit and remaining proof

This is an upstream-only source observation. The local Firefox160 bundle names
mozilla-release source stamp `d84e7a6936554efe26097d9d9b24a0e11330e630`;
the public Hg raw URLs were unavailable and GitHub did not resolve that stamp.
The following files were therefore read at pinned upstream Git
`f898416c560977636760a1fdab1d3f468cb62486`, with URLs/SHA256 in the ignored
`sources/ref-firefox-media/provenance.json`. This is not proof that the installed XUL follows every
same path. Do not substitute current upstream for release disassembly/runtime
identity when qualifying actual lifetime behavior.

- [AppleVTDecoder::OutputFrame](https://github.com/mozilla-firefox/firefox/blob/f898416c560977636760a1fdab1d3f468cb62486/dom/media/platforms/apple/AppleVTDecoder.cpp)
  synchronously wraps CVPixelBufferGetIOSurface in a MacIOSurface and stores it
  in MacIOSurfaceImage before returning from its hardware-image callback.
  [MacIOSurface](https://github.com/mozilla-firefox/firefox/blob/f898416c560977636760a1fdab1d3f468cb62486/gfx/2d/MacIOSurface.cpp)
  increments use in its constructor and decrements in its destructor. This
  supports the proposed initial producer-to-RDD hold, rather than assuming
  that CF retention by itself prevents decoder-slot reuse.
- [MacIOSurfaceTextureData](https://github.com/mozilla-firefox/firefox/blob/f898416c560977636760a1fdab1d3f468cb62486/gfx/layers/opengl/MacIOSurfaceTextureClientOGL.cpp)
  retains that surface and serializes its ID; this path supplies no fence-holder
  ID. The [texture host](https://github.com/mozilla-firefox/firefox/blob/f898416c560977636760a1fdab1d3f468cb62486/gfx/layers/opengl/MacIOSurfaceTextureHostOGL.cpp)
  looks it up into another MacIOSurface and retains it for its renderer. The
  release160 TextureClient/IPC destruction acknowledgement must still be
  checked: these two endpoint holds alone do not establish overlap across IPC.
- [RenderMacIOSurfaceTextureHost](https://github.com/mozilla-firefox/firefox/blob/f898416c560977636760a1fdab1d3f468cb62486/gfx/webrender_bindings/RenderMacIOSurfaceTextureHost.cpp)
  binds each plane and waits for a write fence only when one was supplied.
  Hardware decode completion and kernel cache publication must therefore
  precede our VT callback. A renderer-side optional-fence branch does not
  manufacture the missing decoder completion/cache contract.

Our matching FX CGL0005 patch adds a global use around NV12 plane import/blit
and real glFinish, then drops it only on successful completion. It copies on
GPU into an independent rectangle texture, so later rendering does not use
AVD storage. Failed completion quarantines the hold. Keep this conservative
completion policy until measured replacement fencing is qualified; removing
glFinish because IsInUse appears false would reopen the race.

There are currently two plane import/blit/completion operations per NV12 image.
Their wait time is a profiling candidate, not the demonstrated cause of40ms
callbacks. Record separate GPU-process/CGL wait cost before considering a
nonblocking completion queue. Any alternative must keep both plane holds until
actual GPU completion, handle context loss/process death, and retain the old
safe fallback. Kernel-import qualification must include the whole RDD→IPC→GPU
lease chain, not merely the producer's held-frame CLI test.

FX subsequently matched the installed Breakpad FILE entries to Mozilla revision
`2887d9de1f69c0e9776413da80e7ba0d1225dd9b`. Its separate CA swapchain audit
(`runs/firefox/fx020-source`) identifies a concrete consumer contract: writer
use ends before front publication, pooled old surfaces are excluded by
IOSurfaceIsInUse, and CALayer/WindowServer is expected to hold reader use.
TERM8 owns the CA snapshot/retirement fix. This is a stronger source-contract
candidate for foreign1024px tiles, not runtime attribution of FX011/012. It
does not retroactively make the five upstream decoder files above release
proof. CV frame lifetime and final BGRA CA tile lifetime are separate holds.
