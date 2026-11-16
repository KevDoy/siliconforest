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

### Kernel round of agent GFX2 (14-15 November): video decode and Wi-Fi statistics on the disk system

Two changes, each its own kit build, each booted once and checked before the next (`sf-bootbin-install.sh`
from Linux, `asahi-bless --next`; `m1-vulkan/tools/sf-kernel-cycle.sh` does the whole cycle). Base check
first: the kit rebuilt from the tree gave the installed `56759b9f…` byte for byte.

| Kit | Change | `sf/kernel.bin` | `.old` | Checked on the M1 |
|---|---|---|---|---|
| k1 | `pdavd=1` on the command line (configuration record only; `kern.kernelcacheuuid` unchanged) | `35044741…` | `56759b9f…` | `/dev/pdavd` present; `m1-avd/tools/avd_disk_test.sh run`: H.264 High, HEVC Main, VP9 1080p, 300 pictures each, **900 of 900 identical** to the workstation's software decode (framemd5), 0 errors, timeouts or DART faults; 2.3-2.8 ms per picture |
| k2 | k1 + `if_attach_nx=46` + IO80211Family counting packets out (collection: IO80211Family, PDBwfm) | `7ad3b4a9…` | `35044741…` | 50 MiB pushed into the M1 over SSH: `ipackets` 334 -> 36856, `ibytes` +52.4 MB, `opackets` 197 -> 4196 (k1, same test: all three stay 0). Screen (capture during a 100 MiB push, 01:56Z): Activity Monitor's Network tab "Data received/sec 15.3 MB", totals counting; the dock's EN0 tile "13 M" down, "19 K" up. Activity Monitor's per-process Sent/Rcvd columns stay 0 (another source, not covered) |

- **AVD**: PDAppleAVD was already a member of the collection (`rebase/build-kc.nix`), inert without the
  boot-arg. The decoder firmware `avd-fw-v2-t0.bin` (aurora-silicon/avd-fw, MIT, 49152 bytes, from the
  M1's Linux) was copied to `/Library/Firmware/apple/` on the disk (not in git). Codex was told
  (`runs/m1-coord/requests-for-CODEX.md`). `/dev/pdavd` is root-only (0600), as SF-W says.
- **Wi-Fi receive statistics**: not a missing count in the driver. IO80211Family hands frames to
  `IONetworkInterface::inputPacket`, whose queue flush calls `ifnet_input_extended` with packet and byte
  counts. But en0 was marked `IFCAP_SKYWALK`: macOS's default `if_attach_nx` (0x2f) attaches a Skywalk
  compat netif to every legacy Ethernet interface, and no flowswitch is attached to an interface with the
  legacy transmit model (kernel log: `skip attaching fsw to en0 using legacy TX model`), so the compat
  netif is never activated and input runs through DLIL's own handler, which skips its statistics for
  `IFCAP_SKYWALK` interfaces (`dlil_input.c`: "the stats are already incremented there"). Nobody counted
  them (`SIOCGIFCAP` on the M1: en0 `0x400`). `if_attach_nx=46` is that default without
  `IF_ATTACH_NX_NETIF_COMPAT`: legacy interfaces stay plain legacy, which is what their data path
  already was. Packets out were 0 for another reason: `IONetworkInterface` counts output bytes itself but
  output packets only from the driver's `IONetworkStats`, which no 802.11 driver kept
  (`m1-bwfm/patches/0001`, applied to the tree's IO80211Family). Byte counters read by non-platform
  programs are rounded to 1 KiB by xnu (`rtsock.c`), as on macOS. `m1-bwfm/tools/ifcounters.py` reads
  them on the M1 the way Activity Monitor does (`NET_RT_IFLIST2`).
- `sfvk-careful.sh` lost lines of a command's output at each one-second streaming boundary (`tail -n` of
  a file whose last line was being written); the first AVD comparison showed 3 "missing" pictures per
  clip for that reason. Fixed (lines are streamed by number); the complete output stays on the M1 in
  `/private/tmp/sfvk-careful-<name>.txt`, and the AVD test compares that.
- `e2fsck -fn` before each install: rc 4, the same findings as since the 21:06Z crash (inode 65144,
  blocks 636291-636295, group 17) plus 266 "extent tree could be narrower" optimisations, which are
  not errors. Not repaired.
- **Installed now: k2, `sf/kernel.bin` `7ad3b4a9e6e7e3e1…`** (full hash and how to rebuild it: SF-R, "14-15 November, night"; `install/out/` holds it).
- Revert: `sf-bootbin-install.sh <SFBOOT> --rollback` gives k1 (`35044741…`); the kits are in
  `runs/gfx2/kit-{base,k1,k2}` and the one before in `runs/m1-desktop-010/install-out-before-gfx2-avd-wifi/`.

### Graphics in the default install (agent GFX2, 15 November)

`m1-vulkan/system.nix` + `tools/sf-gpu-install.sh` (also the userland installer's part `gpu`,
`m1-storage/tools/sf_userland_push.sh push gpu`; INSTALL.md says so). On the M1 since 00:19Z:

| Path | What | Before |
|---|---|---|
| `/usr/local/lib/mesa-asahi/libEGL.1.dylib`, `libGLESv2.2.dylib`, `libgallium-26.3.0-devel.dylib` | GL Mesa b7b03c5 (clock + `ETIME` fixes, Wayland EGL platform) | 147a62f, kept as `<name>.old-b7b03c5` |
| `/usr/local/lib/mesa-asahi/libvulkan_asahi.dylib`, `libwayland-client.0.dylib` | Honeykrisp, the client library the new EGL links | new |
| `/usr/local/lib/libvulkan.1.dylib` (+ `libvulkan.dylib`) | Khronos loader 1.4.341 | new |
| `/etc/vulkan/icd.d/asahi_icd.json` | the ICD (the M1's `XDG_DATA_DIRS` is `/usr/share:/share`, so `/usr/local/share` is not searched; the first install put it there and the loader found no driver) | new |
| `/usr/local/bin/vulkaninfo`, `vkcube` (with `SFVK_CUBE_INSTANCES`), `vkcubepp` | tools | new |

`libdrm.2.dylib` stays the installed one (same 212 exports). Records: `/private/var/db/sfgpu/installed-<tag>.txt`
(tags `b7b03c5`, `b7b03c5-etc`, `b7b03c5-cube`); revert: `sf-gpu-install.sh revert <tag>` in reverse order, then
relaunch the compositor.

Observed on the M1 (gui.lock, UART captured, no Vulkan environment variable set):
- the compositor relaunched on the new GL (`WMSTAT selected renderer=gl output=kms width=2560 height=1600`)
  and has run on it since, through a recovery boot; the GL off-screen test passes with the same exact hash
  as with 147a62f (`8fee6b41…`); other agents have run Terminal and Activity Monitor on it since (their holds);
  no Firefox or other GL application is installed on the disk system;
- `vulkaninfo --summary`: Apple M1 (G13G B1), `DRIVER_ID_MESA_HONEYKRISP`, API 1.4.363;
- vkcube in a window: FIFO 300 frames in 6.3 s (with start-up), **MAILBOX 600 frames in 4.5 s** (about
  134 fps), **IMMEDIATE: "Present mode specified is not supported"** (Mesa's Wayland WSI offers no
  immediate mode); the compositor's own check (composited window against the client's buffer at commits
  30 and 150) 4 of 4 identical;
- present race: the cube drawn 1, 300 and 3000 times per frame (`SFVK_CUBE_INSTANCES`), 200 frames each:
  6 of 6 compositor checks identical, 0 differing pixels; but 3000 instances still ran at about 50 fps,
  so these frames were not heavy enough to show a missing fence (heavier runs: below). The compositor's
  counters at the end of a session with 900 dma-buf commits: `fence_waits=0 fence_wait_max_us=27515
  fence_timeouts=0 fence_poll_errors=0` (implicit sync, `poll-dmabuf-before-render`);
- explicit sync: not tried (the compositor has no `wp_linux_drm_syncobj_v1`; not cheap).
- The Vulkan DRM properties do not match the device node: Honeykrisp reports `renderMajor 31,
  renderMinor 22913`, `hasPrimary false`; `/dev/dri/renderD129` is `31, 22657` (card0 `31, 24832`,
  card1 `31, 24577`). Anything that matches a DRM fd to a VkPhysicalDevice by device number (zink on an
  EGL DRM screen, dma-buf feedback) finds none: a lead for "zink does not start", not established.

The drop of 00:53Z (M1 restarted into Linux during a GFX2 hold; recovered at 00:56Z): no panic and no GPU
message; the compositor ended with `PDHID sequence discontinuity: queue overflow or competing reader`
after input at the seat (menus, a window closed; not the inject module, not another agent), then the
session's end restarted the machine. After it `e2fsck -fn` lists new orphan/bitmap findings (not repaired).

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

**Conformance (agent GFX2, 15 November):** dEQP-VK of VK-GL-CTS 1.4.5.3 cross-built for arm64 Darwin
(`m1-vulkan/cts.nix`: target `vulkan_headless` on the CTS's Unix platform code; two seds) and run on the
M1 by `tools/sfvk-cts.sh` against the system install (no environment), UART captured, each group capped:

| Group | Cases | Pass | Not supported | Fail | Crash / hang |
|---|---|---|---|---|---|
| info | 19 | 17 | 2 (memory budget, device groups) | 0 | 0 |
| api.smoke | 6 | 6 | 0 | 0 | 0 |
| api.object_management.single | 47 | 47 | 0 | 0 | 0 |
| memory.allocation.basic | 102 | 102 | 0 | 0 | 0 |
| synchronization.basic (incl. timeline semaphores) | 29 | 21 | 8 (no exclusive compute queue; one queue only) | 0 | 0 |
| compute.pipeline.basic | 78 | 73 | 5 | 0 | 0 |
| draw.renderpass.simple_draw | 4 | 4 | 0 | 0 | 0 |
| api.copy_and_blit.core.image_to_image.simple_tests | 23 | 15 | 8 | 0 | 0 |
| glsl.arrays.constructor | 28 | 28 | 0 | 0 | 0 |
| texture.filtering.2d.formats.r8g8b8a8_unorm | 9 | 6 | 3 | 0 | 0 |
| api.external.memory.dma_buf | 50 (33 reached) | 16 | 15 (no image export; the CTS's Darwin build has no dup()/fd passing) | 1: `dedicated.device_only.export_multiple_times`, `vkEnumeratePhysicalDevices` -> `VK_ERROR_INITIALIZATION_FAILED` | **1 hang: `dedicated.device_only.fd_properties`** (below) |

Groups `api.external.semaphore.sync_fd` and `api.external.fence.sync_fd` were not run: the dma-buf
group hung the device first. Totals: 377 cases ran to a result (335 pass, 41 not supported, 1 fail), 1 hung; no fail outside the dma-buf group,
the fail and the hang in it.

**The hang (01:22Z):** in `fd_properties` (export a dma-buf, then `vkGetMemoryFdPropertiesKHR` on it,
which in Honeykrisp is a PRIME import of the device's own buffer) deqp-vk went into an uninterruptible
wait in the kernel and stayed there (`kill -9` no effect); from then on a new `vulkaninfo` also blocked
uninterruptibly while opening the device. No kernel message, no GPU timeout; the running compositor kept
working. The test before it had failed already at device enumeration, after exporting one memory
object many times. Cleared only by a reboot (01:37Z). Not analysed further: a kernel item (LinuxKPI
PRIME/dma-buf import of an own buffer, or a lock left held after the failed enumeration), for the next
kernel round, announced against CODEX-TB's base first. These are smoke groups chosen by hand, not a conformance submission;
"Not supported" is what the driver reports for this device, not a failure.
The CTS build is ours; the driver's reported conformance 1.4.6.2 is still Omacom's claim for Linux.

**What the Metal experiment (Darling's Indium: Vulkan 1.3, timeline semaphores, dma-buf import)
would find here:** Vulkan 1.4.363 reported; `timelineSemaphore` and `VK_KHR_timeline_semaphore`
reported, and the kernel's timeline syncobj ioctls work (`sync-probe`), but no Vulkan-level timeline
semaphore test was run; `VK_EXT_external_memory_dma_buf` and `VK_KHR_external_memory_fd` reported,
PRIME import of a dma-buf received over a socket works at the kernel level (`sync-probe`), and the
compositor imports Honeykrisp's dma-bufs, but importing a foreign dma-buf into a `VkDeviceMemory`
was not tested. Those two tests are what to write first.

### Open items

- ~~`/dev/pdavd` absent on the disk kernel~~: done (kit k1, see "Kernel round of agent GFX2").
- ~~Install the GL Mesa with the clock and `ETIME` fixes~~: b7b03c5 installed (see "Graphics in the default install").
- A test that would catch a missing implicit-sync fence (a heavy frame per present), and the
  compositor's fence counters; `wp_linux_drm_syncobj_v1` in Chonkstep (explicit sync) would be the
  better protocol, request to DESK3 when wanted.
- MAILBOX works (about 134 fps); IMMEDIATE is not offered by Mesa's Wayland WSI here.
- Conformance: no CTS run (see above).
- Zink on Honeykrisp does not start (EGL falls back to swrast); patch 0201 is experimental.
- **Kernel, second issue: mappings dropped under in-flight jobs** on G13 (GPU faults + firmware recovery
  when a client dies or closes its DRM file with work queued; reproduced 14 November, see "The crash
  reproduced"): in linux-aurora a GEM handle close unmaps at once on G13 (`File::unbind_gem_object` ->
  `Vm::drop_mappings`, and `drm_file_free` releases handles before the driver's `postclose` stops the
  queues); only G15/G17 VMs carry `VmLifetime`, which defers the unmap while accepted jobs may still use
  the object. The fix is to give G13 VMs the same deferral (or to stop and drain a file's queues before
  its handles are released). Not attempted; after taskqueue fix 0041 the faults are recovered, not fatal.
- **Kernel, third issue (15 November): a process that used Indium cannot exit.** Indium's device polling
  thread waits on a timeline semaphore with an infinite timeout (`vkWaitSemaphores(..., UINT64_MAX)`, a
  DRM syncobj timeline wait); at `exit`/`_exit` the process stays in state `?NE` for good (seen twice;
  the GPU stays usable for others). Linux's wait would return `-ERESTARTSYS` when the task is killed;
  our LinuxKPI's interruptible wait is evidently not ended by XNU's thread termination. To check in
  `m1-rust-kext/lkpi` (signal_pending / schedule_timeout against thread_abort) in the next kernel round.
- **Kernel, fourth issue: the dEQP-VK dma-buf hang** (`fd_properties` after `export_multiple_times`):
  uninterruptible, and new device opens blocked until a reboot (see "Conformance"). Not analysed.
- vkcube waits while the display sleeps (no frame callbacks); a compositor policy question for DESK3.
- `memory_limit = 0` from our kernel's GET_PARAMS (newer kernels report it); Honeykrisp falls back
  to the machine's memory for its heap (7.02 GiB).
- The ray-query render path (`drm_asahi_cmd_render_with_ray_query`, 448 bytes) depends on the kernel
  feature `RENDER_RAY_QUERY`, which our kernel does not report, so it is not used; its tail layout
  against the kernel's 448-byte `cmd_render` was not compared.


### 2026-11-15 23:20Z desktop freeze: devfs lookup, captured before recovery

CODEX-TB captured the still-wedged machine before reset under core/gui/bench:
37 tasks/201 threads, kernel stackshot decoded and symbolicated against exact
installed CIO29 and its AGX/DCP maps, ps,23:15+ dmesg, service ioreg and live UART.
Private evidence/analysis: runs/sftb/gpu-wedge-2320/.

Both compositor28999 main73279 and Firefox GPU helper28583 Renderer72355 wait
in devfs_dntovn+0x94/msleep/lck_mtx_sleep on the same channel. The calls are
open/namei and fstatat/namei, before DRM callbacks. The captured AGX/DCP workers
are idle in taskqueue semaphores; services remain attached/busy0. No corresponding
GPU fault/timeout/recovery appears in the retained ring. The kernel is waiting
for DN_CREATE completion, not a DRM mutex held by the exiting helper. Generic
msleep gives no owner; no live creator was identified in the captured stacks.

The installed shared PDDRMDri already returns clone slots on close, but stat-only
lookups allocate cloned vnodes without ever calling open/close. devfs reclaim
never returns those reservations. Actual frozen source under host ASan/UBSan
reproduces1023 leaked slots, then ENOMEM on stat lookup1024; devfs_dntovn's early
failure skips DN_CREATE clear/wakeup, leaving later callers blocked indefinitely.
The historical lookup count and original failing caller were not captured on
hardware. This mechanism matches the observed wait; it does not establish that
the older dEQP dma-buf hang or syncobj wait had the same cause.

Offline fix: creation-error cleanup and actual-minor retirement, interruptible
PCATCH creation wait, opt-in vnode-reclaim clone API, and shared DRM reservation
ownership through final reclaim instead of early close/error reuse. Existing
GPU-file teardown still owns pages/mappings; no in-flight backing is freed to
make exit pass. The earlier taskqueue0041 fix and GPU/DCP drivers are unchanged.
Actual-source tests cover15000 lookup/open lifecycle cycles, pool failure/reuse,
minor1, abort, delayed reclaim, legacy consumers and live-cookie refusal.

The source, frozen native recipes, link verification and next-window gate are in
[m1-devfs-clone/README.md](../../../integrations/puredarwin/m1-devfs-clone/README.md).
This is the highest-priority addition to the SAME future Firefox AVD/K14/HID
install, not another boot now. Native SIGKILL/IPC closure and metadata stress
remain unqualified until an owner-opened window; fixed-image probe is built only.

Recovery returned standard .next/socket/inject and a non-input stats reply on
exact CIO29 at23:37:34Z, UART stopped/locks released. One M5 reset was used.
Read-only e2fsck returned4 with bitmap/count differences, journal replay skipped;
no repairs made. Correction: initial clean request used /sbin/reboot rather
than supported fenced /bin/reboot, so a shutdown-kernel hang was not proved.
No extra recovery boot was used to retest that mistake. CODEX-TB then resumed
its machine pause and continued only workstation work. No AVE or push.
