## SF-AC: Vulkan on the M1 (Honeykrisp from Omacom's Mesa) — hand-over (agent VK, 14 November 2026)

Goal agreed with the owner: Vulkan on the M1 under SiliconForest with Omacom's Mesa ("Omacom has
better vulkan"). Milestones, in order: (1) `vulkaninfo` lists the Apple G13 device; (2) a headless
triangle renders and its readback hash matches a reference; (3) vkcube (or equivalent) in a window
on the Chonkstep compositor with correct presentation sync.

### State

| Milestone | State |
|---|---|
| 1 `vulkaninfo` lists the G13 | **Observed on the M1** (installed SiliconForest, staged, over SSH): GPU0 "Apple M1 (G13G B1)", `DRIVER_ID_MESA_HONEYKRISP` (Omacom names it "Borealis, derived from Honeykrisp"), API 1.4.363, driver-reported conformance 1.4.6.2, 153 device extensions (swapchain, timeline semaphore, external memory dma-buf, DRM format modifier, synchronization2, dynamic rendering, BDA, sparse binding), one 7.02 GiB heap. |
| 2 headless triangle, hash | **Observed on the M1** (`vk-tri`, runs `vk-tri-linear-005` and `vk-tri-copy-006`): `flat_fnv 0x5965baca` = the CPU reference in the program = lavapipe on the builder; the interpolated triangle differs from the CPU reference by 0 (lavapipe: 1); `VKTRI PASS` both with a linear host-visible target (render only) and with an optimal target read back by `vkCmdCopyImageToBuffer` (render + a compute dispatch on Honeykrisp). Numbers: fence after 1.49 ms (first submit), 0.377 ms per submit+wait over 100 frames (copy mode; 0.312 ms render only); device creation 54 ms with a warm shader cache (359 ms cold), pipeline 1.7 ms warm (139 ms cold). |
| 3 vkcube on Chonkstep | **Observed on the M1** (runs `vkcube-007`, `vkcube-008`, on DESK3's running `.next` compositor): Khronos vkcube 1.4.341, Wayland WSI, FIFO: 600 frames in about 10 s and 1200 in about 20 s (60 Hz pacing), exit 0. The compositor imported the swapchain's 8 dma-bufs zero-copy (`XR24`, modifier linear, stride 2048, "EGL image, no copy") and its own check at commits 30 and 150 compared the composited region with the client's buffer: 250,000 pixels, 0 differing, `client-pixels-are-on-the-frame`. Two renderer captures 500 ms apart show the textured LunarG cube complete and rotated (`runs/sfvk/vkcube-008-ab.png`, not committed). |

Presentation sync is implicit sync: Mesa's WSI attaches the render fence to the dma-buf
(`DMA_BUF_IOCTL_IMPORT_SYNC_FILE`, verified by `sync-probe`), and the compositor polls the dma-buf
readable before it renders with it (`poll-dmabuf-before-render`). Not measured: the compositor's
`fence_waits`/`fence_timeouts` counters (printed only when the compositor exits; the compositor is
DESK3's and was not restarted), and a load where a missing fence would show (vkcube's frames are
cheap). The compositor does not offer `wp_linux_drm_syncobj_v1`; nothing was requested from DESK3.

What made it work, in order (each found on the M1):
1. three build fixes for Honeykrisp on Darwin (patches 0101-0103: `vk_image` DRM modifier, ICD link
   arguments, a Mach-O UUID as the build id) — `vkCreateInstance` failed before 0103;
2. `ETIME` = `ETIMEDOUT` in all of Mesa (the kernel's LinuxKPI returns `ETIMEDOUT`);
3. **the clock**: Mesa's absolute DRM deadlines came from Darwin's `CLOCK_MONOTONIC`, which is
   `gettimeofday() - kern.boottime` (µs resolution; checked: 1794692401.974217 - 1794691325.275227
   = 1076.698990 s = the value read at the same moment), about 10.06 s **behind** the kernel DRM
   layer's clock (`pdos_uptime_us`, i.e. mach_absolute_time = `CLOCK_UPTIME_RAW`; why
   `kern.boottime` lies 10 s after that clock's zero was not established). Every finite wait shorter than that expired at once
   (`wait-probe`: a 300 ms wait took 0.0 ms with `CLOCK_MONOTONIC`, 302 ms with `CLOCK_UPTIME_RAW`),
   so `vkWaitForFences` returned `VK_TIMEOUT` (or `VK_ERROR_UNKNOWN` before fix 2) immediately and
   the test exited with its GPU work still in flight. Patch `m1-mesa-asahi/patches/0011` makes
   Mesa's monotonic time `CLOCK_UPTIME_RAW` on Darwin.

The clock, cleaner fix: Linux defines the DRM deadline clock as `CLOCK_MONOTONIC`, which on Linux is
the kernel's `ktime_get`. Darwin has no kernel clock equal to its userspace `CLOCK_MONOTONIC`: that
one is derived from the wall clock and moves when the time is set. The kernel's choice
(mach_absolute_time) is the right one for deadlines; the fix belongs in user space (done in Mesa;
libdrm callers that build deadlines themselves need the same). One caveat for later (SF-K, sleep):
`CLOCK_UPTIME_RAW` stops during sleep and `CLOCK_MONOTONIC_RAW` (mach_continuous_time) does not;
the kernel side would have to say which it counts once the machine sleeps. Nothing in the kernel
was changed for this.

The same two problems (clock, `ETIME`) are in the GL Mesa (147a62f) installed on the M1; the
compositor does not notice because it waits without a deadline. The fixes are in the shared recipe
now (`m1-mesa-asahi/mesa-darwin-build.nix`), so the next GL build carries them; **the installed GL
Mesa was not replaced**. Files that would change on an install: `/usr/local/lib/mesa-asahi/`
`libgallium-26.3.0-devel.dylib`, `libEGL.1.dylib`, `libGLESv2.2.dylib` (not `libdrm.2.dylib`).

The kernel side of synchronisation that the research report listed as missing is **present in
the installed kernel** (`sync-probe`, 15 of 15 on the M1): timeline syncobj signal/query/wait/
transfer, sync_file export/import and `poll()` (POLLIN, not POLLNVAL), syncobj fds, SCM_RIGHTS of
syncobj/sync_file/dma-buf descriptors, `DMA_BUF_IOCTL_EXPORT_SYNC_FILE`/`IMPORT_SYNC_FILE`, `poll()`
on a dma-buf. These are the layer patches 0021/0022/0024 of `m1-rust-kext/lkpi/patches-layer`. The
probe used already-signalled fences; a syncobj wait on pending GPU work woke on completion (vk-tri,
1.5 ms); a `poll()` wake-up on a pending sync_file or dma-buf was not tested on its own.

### How to build and run

`integrations/puredarwin/m1-vulkan/README.md`. In short: `nix-build stage.nix`, then
`tools/sfvk.sh push` copies the tree to `/private/var/db/sfvk` on the M1 (checksums compared);
`tools/sfvk.sh run <cmd>` runs with `VK_ICD_FILENAMES`/`DYLD_LIBRARY_PATH` set; `tools/sfvk-careful.sh`
is the wrapper for any run that could take the machine down (announces in `runs/m1-coord/log.txt`,
syncs, streams the kernel log off the machine every second).

### What is on the M1 and how to revert

- `/private/var/db/sfvk` (the stage: Honeykrisp, loader, vulkaninfo, vkcube, vk-tri, probes, the GL
  off-screen test; `zink/` holds the dev-build GL libraries and gl-bench) and `/private/tmp/sfvk-*`.
  No installed file of the system was replaced by the Vulkan work. Revert: `rm -rf /private/var/db/sfvk*`.
- **The kernel:** `sf/kernel.bin` on SFBOOT is `56759b9f…` (the taskqueue fix) since 22:43Z;
  `sf/kernel.bin.old` is `821cb932…`. Revert from the M1's Linux:
  `sudo bash ./sf-bootbin-install.sh <PARTUUID of SFBOOT> --rollback` (SF-R).
- On the M5: `~/vk-serial/` (the UART capture script and its captures).

### UAPI: kernel `asahi_drm.h` (linux-aurora `6a08a08e`) against Mesa

`m1-vulkan/tools/uapi_compare.py` (clang record layouts, ioctl numbers, constants):

| | Mesa `147a62f` (GL on the M1) | Mesa `b7b03c5` (this Vulkan) |
|---|---|---|
| items equal | 130 of 163 | 148 of 178 |
| ioctl numbers (12) | all equal | all equal |
| `drm_asahi_params_global` | kernel 616, Mesa 600 (size-negotiated tail) | identical (616) |
| `drm_asahi_cmd_render` / `cmd_compute` | kernel 448/200, Mesa 320/128 (size in `cmd_header`) | same |
| constants only in the kernel | 21 (feature bits 17-29, sparse tile, render/compute resources, cumulative signals, ...) | 5: `COMPUTE_WIDE_VISIBILITY`, `RENDER_CACHED_PROGRAM_PROBE`, `RENDER_FRAGMENT_DEPENDENCY(_COMPUTE)`, `VM_CREATE_HAL200` |
| only in Mesa | wrapper structs `*_with_prior`, `*_with_ray_query` | those plus `*_with_stage_prior`, `render_program(_v2)`, `compute_program`, flags `COMPUTE_USES_MATRIX`, `RENDER_COMPUTE_STAGE_BARRIERS`, `RENDER_SHADER_WRITES`, `RENDER_PROGRAM_*` |

All of the Mesa-only items are chosen from kernel feature bits or are G17/M5 paths. Our kernel
reports `features = 0x1` (SOFT_FAULTS only; `agx-params` on the M1), so Honeykrisp takes the
legacy G13 paths: 240-byte render commands, 128-byte compute commands, no exact-prior barriers.

### The crash of 21:06Z (found and fixed below)

What happened (runs/sfvk/vk-tri-00*.txt, runs/m1-coord/log.txt):

| Run (uptime) | What the test did | Kernel |
|---|---|---|
| 1 (~100 s) | submit (render, then compute), fence wait returned at once (clock), `exit(1)` without destroying the device | `firmware consumed its first channel command after 306 us` (the context invalidation of the teardown, first of the boot); nothing else |
| 2 (~402 s) | the same, with `ETIME` fixed: `VK_TIMEOUT` at once, `exit(1)` | nothing in the next ~60 s |
| 3 (~460 s) | `ASAHI_MESA_DEBUG=sync`: an infinite timeline wait inside `vkQueueSubmit`, then as run 2 | the machine stopped answering at ~21:06:30Z (uptime ~500 s) and came back in Linux by itself |

No panic text survived: the serial cable was disconnected then, SiliconForest writes no panic log,
the PMU count was read only after clean reboots. (The hypothesis below was wrong; the reproduction
further down found the real cause.) Lead hypothesis at the time: the death came
about 100 s after run 2, which is the GPU scheduler's job timeout (`drm_sched` timeout 100000 ms
in `queue/mod.rs`); run 2 exited while its job (which takes about 1.5 ms) could still be in
flight, so the scheduler's timeout work may have run after the queue's scheduler was finalised, or
the timeout path (`timed_out` -> `NoDevice` -> `retain_job_on_enodev`, m1-drm72 patch 0005) misbehaved
on a job the teardown had cancelled. linux-aurora's Rust wrapper keeps the scheduler allocation
alive for every job (`Job._scheduler: Arc<SchedulerInner>`) and `drm_sched_fini` cancels the timeout
work, so a fault would be in our C port of the scheduler or in LinuxKPI's delayed work
(`cancel_delayed_work_sync`), not in the driver; reading did not find it.

Since then every GPU run went through `tools/sfvk-careful.sh` with working waits (no test exits
with work in flight now): five runs, no crash, no driver message. The bug is still there for any
process that dies with GPU work queued (a crashing GL or Vulkan app could take the machine down).

The bench cable is back (14 November ~21:50Z) and the disk-booted kernel's console reaches the M5
(`macvdmtool serial`, no reset; `/dev/cu.debug-console`; the installed boot-args have
`serial=0x13 serial-device-name=uart0 debug=0x14e`): a panic's text is now captured by the careful
wrapper. Next step: reproduce deliberately (submit and exit at once) in a RAM-root bench boot,
where a panic costs no disk, read the panic, then fix in one kernel change.

Bench attempt (14 November 22:11-22:18Z, gui.lock, logged): the guest script is
`m1-vulkan/test/bench-teardown.in` (a baseline triangle, then (a) `_exit` right after 200 submits,
(b) the DRM files closed after 200 submits with the process alive, (c) `kill -9` while rendering,
each followed by 130 s of "alive" lines and new kernel log; `vk-tri` has the modes
`VKTRI_EXIT_AFTER_SUBMIT`/`VKTRI_CLOSE_AFTER_SUBMIT`). **Nothing was booted:** `tools/agx_boot.sh
vkb01` failed at RAM-root staging (the 480 MiB image was full: agx_boot stages the GL test's 31 MB
of libraries, the stage added 63 MB), `vkb02` with only the seven files the test needs failed the
stager's "at least 16 MiB free" rule; a third attempt through `tools/pcie_iterate.sh` without the GL
files was not run (refused by the session's permissions; the coordinator's one-retry rule). The M1
went SiliconForest -> Linux (clean `reboot`) -> SiliconForest (`e2fsck -fn` rc 4, the unrepaired
21:06Z state) with the daily compositor as DESK3 had left it. Whether the bench path works since the
disk install is therefore still unknown. A next attempt needs a RAM root with room (a base image
without the desktop payload, or the slim stager `m1-boot-speed/tools/stage_ramdisk_slim.py` that
`tools/gpu_desktop.sh` uses) and `PCIE_EXTRA_FILES` with the seven files.

### The crash reproduced and explained (14 November 22:22-22:27Z, disk system, UART captured)

Session `vk-teardown-*` (runs/sfvk/; announced, gui.lock, `macvdmtool serial` and a capture of
`/dev/cu.debug-console` on the M5 verified with a marker first, `sync` before each trigger):

- **(a) `_exit` right after 200 submits** (about 75 ms of GPU work left in flight): the machine
  survived the 120 s watch. Kernel log: `QueueJob 1664..1668: Job canceled on DRM scheduler
  teardown`, then five `GPU timeout` reports with fault info `reason: Unmapped`, reads of the
  test's shader code (`0x11fffbc000`, unit UL1C), compute data (`0x2fffeac000`, gCDM) and the VDM
  control stream (`0x2fffed0400`, dVDM_CSD) in VM slot 2, each followed by `Attempting recovery...`.
  The process's GEM handles were closed (and their GPU mappings dropped) while the firmware still
  ran its jobs.
- **(b) the DRM files closed after 200 submits, process alive**: the kernel panicked 0.2 s after
  the trigger (`runs/sfvk/vk-teardown-001-uart.txt`): `panic(cpu 5 caller ...): Mutex
  0xfffffe0011d05618 is unexpectedly not owned by thread ... @lock_mtx.c:148`, panicked task
  kernel_task. Symbolized against the installed collection (KC slide `0xb0b8000`; PDAgx's UUID
  `6B3B1C5C...` equal in the panic, the collection and the kext build) and PDAgx's link map:
  `thread_entry` -> `tq_handle_thread` -> `taskqueue_run_locked+0x10c` -> `pdos_mtx_unlock` on
  `Giant`. (c) was not reached.

Cause, found by reading with that stack: Haiku's copy of FreeBSD's taskqueue in our LinuxKPI layer
(`src/Kernel/fbsdkpi/haiku/freebsd_network/fbsd_subr_taskqueue.c`, compiled into every kext built
with `add_fbsdkpi_driver`: PDAgx and PDDcp in the installed collection) reads `task->ta_flags`
**after** `task->ta_func` returns, to decide whether to drop Giant. A LinuxKPI work item embeds its
task, and a Linux work function may free its own work item (the GPU recovery work does); the read
is then a use after free, and freed memory with `TASK_NEEDSGIANT` set unlocks a Giant nobody
holds. FreeBSD's own taskqueue never touches the task after the call. Fix: layer patch
`m1-rust-kext/lkpi/patches-layer/0041` (the flag is read before the call), one change for both
kexts (PDDcp builds from the same patched layer, `m1-dcp/build-kext.nix`). The 21:06Z crash had the
same shape (a process exited with work in flight; no text survived then).

**The fix on the M1 (22:41-23:08Z):** `sf/kernel.bin` `56759b9f…` = the installed kit `821cb932…` (rebuilt
without the patch: byte for byte) plus patch 0041; only PDAgx and PDDcp change in the collection (their
`taskqueue_run_locked` no longer reloads `ta_flags` after the call). Installed with `sf-bootbin-install.sh`
(`kernel.bin.old` = `821cb932…`), one-time boot, up at 22:43Z (`kern.kernelcacheuuid` 742E0857 -> D184D33A).
With the UART captured (`runs/sfvk/vk-teardown-002-uart.txt`, no panic line): variants a, b, c with 200
frames, then with 3000 frames in flight; a with 3000 drove 10 GPU timeouts and 10 firmware recoveries
(the event that preceded the 22:24Z panic) and the machine stayed up through b and c after it. Health
afterwards: the compositor alive (same pid since boot), the GL off-screen test passes by hash
(`agx-offscreen-test --expect-b 8fee6b41…`, installed GL Mesa), vkcube 300 frames in a window. One
observation: while the display is asleep (idle, 300 s) the compositor sends no frame callbacks, so vkcube
(FIFO) waits until the display wakes; that is pacing, not a GPU hang. The new kernel stays installed.

The first event, mappings dropped under jobs the firmware still runs, is not our port's: in
linux-aurora a GEM handle close calls `File::unbind_gem_object` -> `Vm::drop_mappings`, and Linux's
`drm_file_free` calls `drm_gem_release` (handle close) **before** the driver's `postclose` (the
queues). Only G15 and G17 VMs carry `VmLifetime` (`Vm::with_status`/`with_tracked_status`), which
defers handle-close unmaps while accepted jobs can still use the object; G13 VMs unmap at once, so
the same GPU faults are expected on Linux for a process killed with work in flight, and the
firmware recovery handles them. What must be robust is the recovery path, which the fix is about.
Bringing `VmLifetime` to G13 would avoid the faults; not attempted.

### GL, zink, conformance, Metal (after the milestones)

**Native GL vs zink on Honeykrisp** (`m1-vulkan/test/gl-bench.c`: surfaceless EGL, 1024x1024 FBO, 64
opaque rectangles a frame, `glFinish` per frame, exact-image hash against a CPU reference; runs
`gl-zink-compare-00*`, 200 frames, the M1 otherwise idle):

| Driver | ms per frame | fps | image |
|---|---|---|---|
| installed GL Mesa 147a62f (asahi, `/usr/local/lib/mesa-asahi`) | 2.350 | 425.6 | `0x727f9a62` = reference |
| Mesa b7b03c5 asahi (dev build of this stage) | 2.293 | 436.1 | `0x727f9a62` = reference |
| Mesa b7b03c5 zink on Honeykrisp | **does not run** | | |

Zink needed a build change for Darwin without MoltenVK (`m1-vulkan/patches/experimental/0201`, applied by
no recipe; it builds). On the M1, `MESA_LOADER_DRIVER_OVERRIDE=zink` makes EGL's DRM screen fail
("DRI2: failed to load driver") and EGL falls back to surfaceless swrast (softpipe, minutes per run);
why zink refuses the device was not established. So the comparison is native GL only for now; no
change to the compositor's renderer was considered.

**Conformance:** no dEQP-VK / CTS run. Building VK-GL-CTS for arm64 Darwin needs a non-Cocoa platform
target (Vulkan-only, no WSI) and its third-party tree (glslang, SPIRV-Tools, amber) in the cross
toolchain; not attempted in this session. The driver reports conformance 1.4.6.2, which is Omacom's
claim for its own Linux build, not a result on this system.

**What the Metal experiment (Darling's Indium: Vulkan 1.3, timeline semaphores, dma-buf import)
would find here:** Vulkan 1.4.363 reported; `timelineSemaphore` and `VK_KHR_timeline_semaphore`
reported, and the kernel's timeline syncobj ioctls work (`sync-probe`), but no Vulkan-level timeline
semaphore test was run; `VK_EXT_external_memory_dma_buf` and `VK_KHR_external_memory_fd` reported,
PRIME import of a dma-buf received over a socket works at the kernel level (`sync-probe`), and the
compositor imports Honeykrisp's dma-bufs, but importing a foreign dma-buf into a `VkDeviceMemory`
was not tested. Those two tests are what to write first.

### Open items

- `/dev/pdavd` absent on the disk kernel (Codex's finding, `runs/m1-coord/requests-for-VK.md`): PDAppleAVD is in
  the collection but its `start` returns early without the boot-arg `pdavd`; the installed kernel.bin's
  configuration has none. The next, separate kernel change: append `pdavd=1` to the final `ARGS` of
  `m1-native-boot/install/build.sh` (or `SF_EXTRA_BOOTARGS=pdavd=1`), install with the rollback kept;
  the first open then needs `/Library/Firmware/apple/avd-fw-v2-t0.bin` (MIT, aurora-silicon/avd-fw).
  Not done here.
- Install the GL Mesa with the clock and `ETIME` fixes (147a62f + 0011 + `pd-etime.h`); not done.
- A test that would catch a missing implicit-sync fence (a heavy frame per present), and the
  compositor's fence counters; `wp_linux_drm_syncobj_v1` in Chonkstep (explicit sync) would be the
  better protocol, request to DESK3 when wanted.
- vkcube's pacing at 60 Hz is FIFO through frame callbacks; `VK_PRESENT_MODE_MAILBOX/IMMEDIATE`
  not tried.
- Conformance: no CTS run (see above).
- Zink on Honeykrisp does not start (EGL falls back to swrast); patch 0201 is experimental.
- Mappings dropped under in-flight jobs on G13 (GPU faults + recovery when a client dies with work
  queued): bring linux-aurora's `VmLifetime` to G13 VMs, or keep it as Linux does.
- vkcube waits while the display sleeps (no frame callbacks); a compositor policy question for DESK3.
- `memory_limit = 0` from our kernel's GET_PARAMS (newer kernels report it); Honeykrisp falls back
  to the machine's memory for its heap (7.02 GiB).
- The ray-query render path (`drm_asahi_cmd_render_with_ray_query`, 448 bytes) depends on the kernel
  feature `RENDER_RAY_QUERY`, which our kernel does not report, so it is not used; its tail layout
  against the kernel's 448-byte `cmd_render` was not compared.
