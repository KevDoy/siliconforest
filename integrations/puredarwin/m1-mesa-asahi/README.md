# m1-mesa-asahi: Mesa's asahi driver for the PureDarwin M1 guest (SF-G items G1, G5)

Everything in this directory is produced as build and link evidence on the x86-64 Linux
builder. The off-screen test (section 3) has since been run on the M1 by the lead, under Linux
and under Darwin; those results are in `reports/sprint-010/gpu-first-render-001.md`, not here.
The panel demo (section 6) has **not** run on the target: it has only run on the builder, on
llvmpipe and on Mesa's asahi drm-shim (the real asahi driver with no GPU behind it).

## 1. Findings

### Source pins

- `sources/gpu-plan/mesa` is at `a1969111`, **not** at `147a62f6190`. The recipes here fetch
  `omacom/mesa` `147a62f619001f5f2588c7417c2b1fac3b0d93ad` (VERSION `26.3.0-devel`) as a GitHub
  archive tarball, sha256 `15b27d00ba2d857837001d357f189e64809b27937eba0dd722c9d28ec4ad0dcf`
  (`common.nix`). It is no longer the tip of `integration/aurora` (now `b7b03c5`).
- `asahi_drm.h`, kernel `linux-aurora` 6a08a08e vs this Mesa: the files differ textually, the
  ABI does not for anything the M1 uses. Compiling both and comparing `sizeof` of every
  `struct drm_asahi_*`, every `DRM_IOCTL_ASAHI_*` number and the offsets of all
  `drm_asahi_params_global` fields gives three differences, all size-negotiated tails:

  | struct | kernel | Mesa | how it is negotiated |
  |---|---|---|---|
  | `drm_asahi_params_global` | 616 | 600 | kernel copies `min(asked, known)`; tail is `memory_limit`, `client_memory_limit` |
  | `drm_asahi_cmd_render` | 448 | 320 (+ wrapper structs to 448) | `drm_asahi_cmd_header.size`; on G13 Mesa sends **240** |
  | `drm_asahi_cmd_compute` | 200 | 128 (+ wrapper to 200) | same; Mesa sends **128** |

  All 12 ioctl numbers and all other struct sizes are identical.

### What PureDarwin already has for DRM user space

| Piece | Where | State |
|---|---|---|
| `/dev/dri` character device for every DRM kext | `src/Kernel/Extensions/PDDRMDri/PDDRMDri.cpp`, `src/Kernel/fbsdkpi/drm/src/pd_drm_cdev.c` | in tree, used by the Adreno (`PDDRMmsm`) and x86 drivers |
| libdrm port | `nix/pkgs/x11/libdrm.nix` + `src/Libraries/libdrm/puredarwin-dri.patch` (207 lines) on libdrm 2.4.133; flake output `libdrm-dri-arm64` | builds here unchanged; has the platform-bus lookup asahi needs |
| Mesa cross-build | `nix/pkgs/mesa/mesa.nix` (upstream 26.1.6; freedreno/radeonsi/nouveau variants, LLVM, Wayland) | no asahi variant; its `--replace-fail` edits target 26.1.6 |
| GEM mmap on the device fd | `pd_cdev_mmap_register` in `xnu/bsd/kern/kern_mman.c` | in tree |
| Same-open-file test (Linux `kcmp`) | `fcntl(fd1, F_PD_SAMEFILE /* 0x5044 */, fd2)` | in tree |
| Off-screen GLES test | `src/Userspace/pdgltri` (surfaceless EGL, one triangle) | in tree, flake output `pdgltri-msm-arm64` |
| Host shader tools for a cross build | `mesa-pvr-native-tools` (Mesa 26.0.3) | wrong Mesa for asahi; `mesa-native.nix` here builds them from the pinned source |

### Conventions a Mesa winsys must follow on this OS (read from source)

| Topic | PureDarwin | Difference from Linux |
|---|---|---|
| Node names | `/dev/dri/renderD<N>`, `/dev/dri/card<N>`; N = the kext's DRM minor + its provider index, so numbering is system-wide | same names; never assume 128 belongs to the asahi kext. The test scans 128..135 for the name `asahi` |
| major/minor | one dynamic major (`sysctl kern.pd_dri.major`); `dev_t` is 8-bit major, 24-bit minor; an *open* node is a clone whose minor is `(slot << 8) \| drm_minor` | `fstat(fd).st_rdev` differs from `stat(path).st_rdev`; compare `minor & 0xff` (libdrm's patch does). No `<sys/sysmacros.h>`: `compat/sys/sysmacros.h` |
| Enumeration | no sysfs, no udev. `sysctl kern.pd_dri.devices`: one line per minor, `"<minor> platform:<of full_name> <first compatible>"` for device-tree devices, `"<minor> pci:... vendor device ..."` otherwise | libdrm's `drmGetDevices2`/`drmGetDevice2` are patched to read it and report `DRM_BUS_PLATFORM`. Mesa's EGL device list and loader need nothing else |
| ioctl numbers | BSD `<sys/ioccom.h>`: `IOC_OUT 0x40000000`, `IOC_IN 0x80000000`, 13-bit size | `_IOW` requests have a different number (table in section 4); `_IOWR` and all sizes used here are the same. Checked in the built binaries: the Darwin driver contains `0x80186440`, `0x80086409`, `0x80086449`, `0x8028644a` |
| ioctl size limit | XNU copies `IOCPARM_LEN(cmd)` bytes in and out itself, at most 8191 | largest argument used is 64 bytes (`DRM_IOCTL_VERSION`) |
| Nested pointers | the main argument reaches the driver as a kernel copy; pointers inside it (`drm_version.name`, `get_params.pointer`, `vm_bind.userptr`, `submit.syncs`/`cmdbuf`, `syncobj_wait.handles`) are real user addresses | the kernel side must copy them from the calling task |
| mmap | `mmap(NULL, len, PROT_READ\|PROT_WRITE, MAP_SHARED, drm_fd, offset)`; `offset` and `len` must be multiples of 16 KiB; the kernel asks the kext for the physical pages once, at mmap time, and maps them through a device pager | `MAP_PRIVATE` on the node fails (`ENODEV`); pages are fixed for the life of the mapping. Mesa's asahi rounds every BO to 16 KiB already (`AIL_PAGESIZE`) |
| `poll`/`select` on the DRM fd | readiness only, no wakeup (`pd_dri_poll`) | not used by asahi off-screen |
| syncobj | DRM core `drm_syncobj.c` is in PureDarwin's DRM source list; all access is by ioctl on the DRM fd | none expected |
| sync files | `pd_sync_file.c`: fds are `pd_lfile` objects; merge and file-info ioctls exist; `poll()` returns `POLLNVAL`, `select` never wakes; cannot be sent over a socket | the off-screen path creates one sync file per `glFinish` and hands it straight back to the DRM fd (section 4); it never polls it |
| dma-buf / PRIME | export/import exist; dma-buf `mmap` and the sync-file ioctls do not | not used off-screen (0 PRIME calls in the trace) |
| `memfd`/shm | no `memfd_create` (configure: NO); Mesa falls back to `mkstemp`-style anonymous files | asahi off-screen creates none |

### What Mesa's asahi needs from the OS beyond DRM

Configure results for the Darwin target and the code paths they select (all compile and link;
none has run):

- Threads: pthreads; `u_cnd_monotonic` uses `pthread_cond_timedwait_relative_np`. No futex:
  `simple_mtx` falls back to `mtx_t`. The driver starts a BO-cache trim thread and Mesa its
  shader-cache/util queues.
- Time: `clock_gettime(CLOCK_MONOTONIC)` (see risk 5).
- Shader cache identity: this Mesa has a Mach-O path in `src/util/build_id.c` (`LC_UUID` found
  through `dladdr`); `dl_iterate_phdr` is absent and not needed. The built
  `libgallium-26.3.0-devel.dylib` carries an `LC_UUID`.
- CPU detection: `sysctlbyname` path for Apple; memory size via `host_statistics64`.
- `os_same_file_description`: needs patch 0003 (no `kcmp`).
- Not available and not needed: `memfd_create`, `posix_fallocate`, `getrandom`,
  `pthread_setaffinity_np`, `reallocarray`, `secure_getenv` (Mesa has fallbacks).
- No LLVM in the target build: asahi compiles NIR to AGX itself, and its OpenCL C library
  (`libagx`) is compiled at build time by `mesa_clc`, `vtn_bindgen2` and `asahi_clc`, which
  `mesa-native.nix` builds for the build machine.
- Imports of the built driver are all satisfied by libSystem, libc++/libc++abi and libdrm
  (linked without `-undefined dynamic_lookup`).

## 2. Build

```
export PATH=$HOME/.nix-profile/bin:$PATH
F="--extra-experimental-features nix-command --extra-experimental-features flakes"
cd integrations/puredarwin/m1-mesa-asahi
nix-build $F mesa-native.nix --no-out-link              # host tools + Linux asahi + drm-shim
nix-build $F mesa-darwin.nix --no-out-link              # arm64 Darwin Mesa
nix-build $F agx-offscreen-test.nix --no-out-link       # arm64 Darwin test bundle
nix-build $F agx-offscreen-test-linux.nix --no-out-link # builder-side test + ioctl tracer
```

| Component | Result | Store path (this session) |
|---|---|---|
| libdrm (PureDarwin's port, flake `libdrm-dri-arm64`, unmodified) | builds, links | `/nix/store/nr6pc63d1v4air0aajnaz029bk775kp4-puredarwin-libdrm-amdgpu-2.4.133` |
| Host tools `mesa_clc`, `vtn_bindgen2`, `asahi_clc`; Linux x86-64 asahi + llvmpipe + softpipe, surfaceless EGL, `libasahi_noop_drm_shim.so` | builds, links, runs on the builder | `/nix/store/3x55f3rsqbhnx90a4fxk89vpix187c6z-darwin-lab-mesa-asahi-native-26.3.0-devel-147a62f6190` |
| `src/asahi` (isa, genxml, libagx, compiler, clc, layout, lib) for arm64 Darwin | builds | part of the next row |
| Gallium `asahi` + `softpipe`, winsys, EGL (surfaceless), GLES2/3: `libgallium-26.3.0-devel.dylib` (31 MB), `libEGL.1.dylib`, `libGLESv2.2.dylib` | builds, links (1203 ninja steps, 5 patches) | `/nix/store/xz1bzz7vr50xvqygzksk9zvx1s7r885a-darwin-lab-mesa-asahi-arm64-darwin-26.3.0-devel-147a62f6190` |
| `agx-offscreen-test` and `agx-panel-demo` Mach-O arm64 + relocated library bundle | builds, links | `/nix/store/jmq22ikc80xv8ffyqhca9xlmxpnmixhk-darwin-lab-agx-offscreen-test-arm64-darwin` |
| Builder-side test, panel demo, tracer, `run-shim`, `run-sw` | builds, runs | `/nix/store/22k517ii7vlaz5ri0i42755bbviavqsv-darwin-lab-agx-offscreen-test-linux` |
| GBM, Vulkan (honeykrisp), Wayland/X11 platforms | not attempted (`-Dgbm=disabled`, no Vulkan drivers, `-Dplatforms=`) | |

The Darwin test is dynamically linked; that is how PureDarwin's own Mesa is used
(`pdgltri`), and Mesa no longer `dlopen`s per-driver modules: `libEGL` links `libgallium`
directly. The bundle keeps clear of the image's `/usr/lib` Mesa:

```
usr/local/bin/agx-offscreen-test
usr/local/bin/agx-panel-demo           # section 6; PDGOP and its IOKit linked in statically
usr/local/lib/mesa-asahi/{libEGL.1,libGLESv2.2,libgallium-26.3.0-devel,libdrm.2}.dylib
LINKAGE.txt        # otool -L of all six after relocation
```

It also needs `/usr/lib/libSystem.B.dylib`, `libc++.1.dylib`, `libc++abi.dylib` and
`libIOKitCF.dylib` (the last through libdrm) from the image; whether the M1 root has
`libIOKitCF.dylib` was not checked.

### Patches (`patches/`, applied by `mesa-darwin.nix`; the native build uses none)

| File | Lines | Reason |
|---|---|---|
| `0001-meson-darwin-is-a-drm-system.patch` | 21 | Darwin counts as a KMS/DRM system and uses the `drm` DRI platform instead of XQuartz's `apple` |
| `0002-asahi-no-macos-iokit-wrap-tooling.patch` | 12 | keep `src/asahi` off its macOS reverse-engineering branch (`wrap.c`, Apple's AGX user client) when an IOKit framework is found |
| `0003-os-file-same-file-via-F_PD_SAMEFILE.patch` | 14 | `os_same_file_description` through PureDarwin's fcntl; there is no `kcmp` |
| `0004-u-screen-query-DRM_CAP_PRIME-on-darwin.patch` | 11 | ask the kernel for `DRM_CAP_PRIME` on Darwin as on Linux, so the call sequence matches the Linux reference |
| `0005-asahi-ioc-field-accessors-for-bsd-ioccom.patch` | 20 | `_IOC_DIR/_IOC_TYPE/_IOC_NR/_IOC_SIZE` for BSD `ioccom.h`; `agx_device.c` re-sizes `QUEUE_CREATE` with them |

Not a source patch: `compat/sys/sysmacros.h` (16 lines) on the include path, for
`src/loader/loader.c`. Patches 0001, 0003 and 0004 are the same edits PureDarwin's `mesa.nix`
makes with `substituteInPlace` for its other DRM drivers, here as patch files against this
source.

## 3. Test program: `test/agx-offscreen-test.c`

One C file, the same source on Linux and Darwin (libdrm, EGL, GLESv2).

1. Finds the render node whose `drmGetVersion` name is `asahi` (or `--node`), prints name,
   version, date, description; issues `DRM_IOCTL_ASAHI_GET_PARAMS` group 0 asking for 616 bytes
   and prints every field under its uAPI name, core masks, and the feature bits by name.
2. Surfaceless EGL, GLES2 context, 256x256 RGBA8 texture as FBO colour attachment.
3. Frame A: clear to (0.2, 0.4, 0.6, 1.0), `glFinish`, `glReadPixels`.
4. Frame B: same clear, a gradient triangle (red/green/blue corners) on the left half, a quad
   textured from a 4x4 `GL_NEAREST` texture on the right half, `glFinish`, `glReadPixels`.
5. Prints SHA-256 of both frames' RGBA bytes and the sampled pixels; exit 0 pass, 1 check
   failed, 2 could not set up.

### Pass criterion

| Check | Kind | Why it is valid across drivers |
|---|---|---|
| Frame A equals 65536 x (51,102,153,255); hash `ec9ff14546d41e0cf8f3856592a16639cceb74d293fb165a18806d3728cae3f7` | exact | the clear values are exactly representable in unorm8, no rasterisation involved |
| Frame B: 4 background points | exact | untouched by geometry |
| Frame B: 5 points inside the triangle against the barycentric colour computed in the test | +/-3 per channel | interpolation precision differs between rasterisers |
| Frame B: centre of each of the 16 texel blocks of the quad | +/-1 | nearest sampling far from texel edges |
| Frame B: 27648 +/- 600 non-background pixels | range | edge-rule differences only move edges |
| Frame B hash equals `--expect-b` | exact | only against the **same driver**: the hash the same source prints under Linux on this M1 |
| `--require-renderer "Apple M1"` | guard | catches a silent fall-back to softpipe; a renderer string alone is not evidence |

Software renderers are not a pixel reference for frame B's hash on the GPU. For what it is
worth, llvmpipe and softpipe on the builder both pass every check and both give frame B
`90e2f4ba629c368f5273790c69012fb07ff8c368928d295a4dd50d6007c8374d`. Ticket G5's wording "pixel
hash equals the software-renderer reference" is therefore only safe for frame A; for frame B
use the tolerance checks plus the Linux asahi hash.

### The Linux reference (for the lead; run on the M1's Linux, mesa-aurora 26.3.0 asahi installed)

Copy `test/agx-offscreen-test.c` there, then:

```
cc -O2 -o agx-offscreen-test agx-offscreen-test.c $(pkg-config --cflags --libs egl glesv2 libdrm)
./agx-offscreen-test --require-renderer "Apple M1" --dump-b frame-b-linux.rgba | tee agx-offscreen-linux.txt
```

(without the `.pc` files: `-I/usr/include/libdrm -lEGL -lGLESv2 -ldrm`). Keep the whole output:
the `drm version` and `params` lines are what F14's `GET_PARAMS` must reproduce, and the
`frame B ... sha256=` value is the `--expect-b` argument on Darwin. An ioctl log from the real
kernel, to compare with section 4:

```
strace -f -e trace=ioctl,mmap -o agx-offscreen-linux.strace ./agx-offscreen-test
```

### On PureDarwin (once F14 exists)

Copy the bundle's `usr/local` into the root, then:

```
/usr/local/bin/agx-offscreen-test --require-renderer "Apple M1" --expect-b <frame B sha256 from Linux>
MESA_SHADER_CACHE_DISABLE=1 ...            # first runs: one variable fewer
LIBGL_ALWAYS_SOFTWARE=1 /usr/local/bin/agx-offscreen-test --no-drm   # softpipe: tests EGL/libSystem without the GPU
```

The softpipe line needs no kernel driver and can be tried today; it has not been.

### On the builder (no GPU)

```
T=$(nix-build $F agx-offscreen-test-linux.nix --no-out-link)
$T/bin/run-sw                 # llvmpipe: PASS
$T/bin/run-shim trace.txt     # asahi on the drm-shim: FAIL by design (all pixels 0), writes the ioctl log
python3 trace/summarize.py trace.txt
```

## 4. For SF-F F14: what the off-screen test asks of the kernel

Source: `trace/shim-g13g-trace.txt` (184 lines, every ioctl and device mmap in order) and
`trace/shim-g13g-summary.txt`, recorded with `trace/ioctl-trace.c` from the Linux build of this
Mesa running the test on `libasahi_noop_drm_shim.so` with `ASAHI_SHIM_CHIP=g13g`. The shim is
not the kernel driver: it reports `features=0`, completes every submission at once and returns
no fd for a sync-file export. The tracer supplies a placeholder fd there so that the rest of
that path is recorded. The lines marked (code) below come from reading the driver, not from
the trace.

### Requests and their numbers

| Request | Linux | Darwin | Arg bytes | Calls |
|---|---|---|---|---|
| `DRM_IOCTL_VERSION` | `0xc0406400` | same | 64 | 20 (2 test, 18 EGL/loader) |
| `DRM_IOCTL_GEM_CLOSE` | `0x40086409` | `0x80086409` | 8 | 25 |
| `DRM_IOCTL_GET_CAP` (`DRM_CAP_PRIME` = 5) | `0xc010640c` | same | 16 | 1 |
| `DRM_IOCTL_SYNCOBJ_CREATE` | `0xc00864bf` | same | 8 | 10 (one with `CREATE_SIGNALED`) |
| `DRM_IOCTL_SYNCOBJ_DESTROY` | `0xc00864c0` | same | 8 | 10 |
| `DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD` (`EXPORT_SYNC_FILE`) | `0xc01864c1` | same | 24 | 2 |
| `DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE` (`IMPORT_SYNC_FILE`) | `0xc01864c2` | same | 24 | 2 |
| `DRM_IOCTL_SYNCOBJ_WAIT` | `0xc02864c3` | same | 40 | 6 |
| `DRM_IOCTL_ASAHI_GET_PARAMS` | `0x40186440` | `0x80186440` | 24 | 2 (616 bytes from the test, 600 from Mesa) |
| `DRM_IOCTL_ASAHI_VM_CREATE` | `0xc0186442` | same | 24 | 1 |
| `DRM_IOCTL_ASAHI_VM_BIND` | `0x40186444` | `0x80186444` | 24 | 28 |
| `DRM_IOCTL_ASAHI_GEM_CREATE` | `0xc0186445` | same | 24 | 25 |
| `DRM_IOCTL_ASAHI_GEM_MMAP_OFFSET` | `0xc0106446` | same | 16 | 22 |
| `DRM_IOCTL_ASAHI_GEM_BIND_OBJECT` | `0xc0286447` | same | 40 | 2 |
| `DRM_IOCTL_ASAHI_QUEUE_CREATE` | `0xc0186448` (**24** bytes, struct is 40) | same | 24 | 1 |
| `DRM_IOCTL_ASAHI_QUEUE_DESTROY` | `0x40086449` | `0x80086449` | 8 | 1 |
| `DRM_IOCTL_ASAHI_SUBMIT` | `0x4028644a` | `0x8028644a` | 40 | 4 |
| `mmap` of the device fd | | | | 22, all `addr=NULL`, `MAP_SHARED`, read+write, 16 KiB to 1 MiB |

Not called by this test: `ASAHI_VM_DESTROY`, `ASAHI_GET_TIME`, `ASAHI_GEM_MADVISE`, any PRIME
or dma-buf ioctl, `SYNCOBJ_TIMELINE_WAIT`/`QUERY`/`TRANSFER`/`RESET`/`SIGNAL`, `poll`.
The VM and the remaining objects are released by closing the fd.

### Call order

1. Test: `open(renderD*)`, `VERSION` x2 (lengths, then strings), `GET_PARAMS` group 0, `close`.
2. EGL initialise: device enumeration through libdrm (on Darwin `opendir(/dev/dri)`, `stat`,
   `sysctl kern.pd_dri.*`; on Linux sysfs), `VERSION` x16 on two short-lived fds.
3. Screen create on a new fd: `VERSION` x2, `GET_PARAMS` group 0 (600), `VM_CREATE`
   (`kernel_start`/`kernel_end` = top 32 GiB below `vm_end`), then three fixed-address objects,
   each `GEM_CREATE(VM_PRIVATE)` + `VM_BIND` x2, two of them also `GEM_MMAP_OFFSET` + `mmap`:
   zero page at GPU VA `0x100000000` (read-only bind, 64 KiB), scratch page at `0x100010000`,
   printf buffer at `0x1000000000`. Then `SYNCOBJ_CREATE` (the screen's timeline syncobj),
   `GET_CAP(PRIME)`.
4. Context create: BO create/bind/map, `QUEUE_CREATE` (flags 0, priority 1, `usc_exec_base`
   set, 24-byte form), a shared object `GEM_CREATE(WRITEBACK, vm_id 0)` + `VM_BIND` +
   `GEM_BIND_OBJECT(op BIND, usage TIMESTAMPS)`, `SYNCOBJ_CREATE` x2 (one `CREATE_SIGNALED`).
5. Per frame (twice):
   - more `GEM_CREATE` + `VM_BIND` + `GEM_MMAP_OFFSET` + `mmap` (over the whole run: 21
     objects `VM_PRIVATE`, 3 `VM_PRIVATE|WRITEBACK`, 1 shared `WRITEBACK`);
   - `SYNCOBJ_CREATE`, `SUBMIT` with commands `[SET_FRAGMENT_ATTACHMENTS (24 bytes),
     RENDER (240 bytes)]`, 0 in-syncs, 3 out-syncs: a binary syncobj, the screen syncobj as a
     **timeline point**, a second binary syncobj; `SYNCOBJ_DESTROY`;
     `SYNCOBJ_WAIT(timeout 0)` as a poll;
   - `glFinish`: `SYNCOBJ_HANDLE_TO_FD(EXPORT_SYNC_FILE)`, `SYNCOBJ_CREATE`,
     `SYNCOBJ_FD_TO_HANDLE(IMPORT_SYNC_FILE)`, `close(sync fd)` (code),
     `SYNCOBJ_WAIT(absolute timeout INT64_MAX, WAIT_ALL)`, `SYNCOBJ_DESTROY`;
   - `glReadPixels`: `SYNCOBJ_CREATE`, `SUBMIT` with `[COMPUTE (128 bytes)]`, 3 out-syncs,
     `SYNCOBJ_DESTROY`, `SYNCOBJ_WAIT(timeout 0)`; on a real kernel this poll is followed by a
     blocking `SYNCOBJ_WAIT(INT64_MAX)` when the job has not finished (code:
     `agx_sync_batch`), then the CPU reads the result through an existing mapping.
6. Teardown: `SYNCOBJ_DESTROY` x3, `GEM_BIND_OBJECT(op UNBIND)`, `GEM_CLOSE`, `QUEUE_DESTROY`,
   `SYNCOBJ_DESTROY`, `GEM_CLOSE` x24, `close`.

All 28 `VM_BIND`s carry one op of 32 bytes: 27 with flags `READ|WRITE`, 1 with `READ`; no
`UNBIND`, no `SINGLE_PAGE`. Every frame, including the clear-only one, submits one render
command and then, from `glReadPixels`, one compute command.

If the real `GET_PARAMS` sets `DRM_ASAHI_FEATURE_VM_STATUS`, Mesa also calls `GET_PARAMS`
group 1 (code: `agx_drm_vm_status`); the shim sets no feature bits, so the trace does not
show it. `GEM_MADVISE` can appear from the BO cache on longer runs (code).

## 5. Risks, most serious first

1. **ioctl dispatch by BSD number and by size.** Five of the requests above have a different
   number on Darwin, and `QUEUE_CREATE` arrives 16 bytes short of the kernel's struct (as it
   does on Linux for a G13). The kernel side must dispatch on the request's number field with
   BSD direction bits, zero-extend a short argument to the driver's struct, and not write past
   the caller's size. A Rust ioctl table built with Linux's `_IOW` values will not match
   `0x80...` requests.
2. **Sync objects carry the whole completion path.** Needed: binary syncobjs incl.
   `CREATE_SIGNALED`; a timeline point as a `SUBMIT` out-sync (fence chain); `SYNCOBJ_WAIT`
   that really blocks and is woken by job completion, with `WAIT_ALL`, timeout 0 and
   `INT64_MAX`; sync-file export and re-import per `glFinish` (the fd is a `pd_lfile`; it is
   only passed back by ioctl, so the missing `poll`/kqueue support does not matter here). If
   the export fails Mesa prints `export failed` and `glFinish` returns **without waiting**;
   the following readback still waits on its own batch.
3. **GEM mmap pins.** PureDarwin's mmap hook hands XNU physical page numbers once. Every page
   of the object must be populated at `mmap` time and stay put until the mapping goes
   (release callback), including for objects the GPU writes and the CPU then reads (the
   readback target, the timestamp and printf buffers). Offsets from
   `GEM_MMAP_OFFSET` must be 16 KiB multiples. `MAP_FIXED` on this path is unused by the test
   (all 22 mmaps have `addr=NULL`) and remains untested.
4. **`GET_PARAMS` must equal Linux on this M1**, field for field: Mesa derives its GPU VA
   layout from `vm_start`/`vm_end`/`vm_kernel_min_size` (and binds at fixed GPU VAs
   `0x100000000`, `0x100010000`, `0x1000000000`), picks the shader set from
   generation/variant/`chip_id`, sizes submissions from `max_commands_per_submission`, and
   enables paths from `features`. `drmGetVersion` must say `asahi` with Linux's
   major.minor (`0.0.0` in `drivers/gpu/drm/asahi/driver.rs`; Mesa switches a render-command
   field on at 0.2). Mesa cannot detect a short reply: the request is write-only, it assumes
   it got the 600 bytes it asked for, and fields the kernel did not write read as zero.
5. **Two monotonic clocks.** `SYNCOBJ_WAIT` timeouts are absolute `CLOCK_MONOTONIC`
   nanoseconds. User space computes it as wall time minus boot time
   (libSystem `clock_gettime`); LinuxKPI's `ktime_get` comes from `pdos_uptime_us`. This test
   only uses 0 and `INT64_MAX`, so it is unaffected (provided `INT64_MAX` is clamped, not
   overflowed, on the way to a jiffies timeout); any finite wait
   (`glClientWaitSync`, a compositor frame) depends on the two agreeing.
6. **Nested user pointers and W-only requests.** `GET_PARAMS`, `VM_BIND` and `SUBMIT` are
   `_IOW`: XNU copies nothing back, and the results (`GET_PARAMS` data) or inputs (bind ops,
   sync array, command buffer) travel through pointers inside the argument, which are real
   user addresses, not addresses in PDDRMDri's kernel copy of the argument.
7. **Platform-device identity.** libdrm classifies the node from the `kern.pd_dri.devices`
   line. The asahi DRM device must be a platform device with an `of_node` whose `full_name`
   and `compatible` are set, or the line is emitted in PCI form and Mesa's loader takes the
   PCI-id path. The node gets `renderD(128 + provider index)`.
8. **Compute is required.** The readback after each frame is a compute submission, so F15
   cannot pass with the render path alone.
9. **Unverified on the target:** softpipe and EGL start-up on PureDarwin's libSystem, the
   shader disk cache (needs a writable cache directory; disable it at first), thread creation
   from the driver, and the presence of `libIOKitCF.dylib` in the M1 root.
10. **Diagnostics.** Mesa's asahi prints errors with glibc's `%m`; Darwin's `printf` does not
    expand it, so failed-ioctl messages lose the errno text. `agx_fallback_wait` (failure
    paths only) calls `poll` on a sync file, which returns `POLLNVAL` here.
11. **Pin drift.** `sources/gpu-plan/mesa` (`a1969111`) is not the commit built here or
    running on the M1; anyone reading that tree is reading a different driver.

## 6. On-screen demo and benchmark: `test/agx-panel-demo.c`

GLES3 on surfaceless EGL, rendering into FBOs at panel resolution, shown on the built-in panel
through the same CPU path the compositor uses: `PDGOPOpen` (64-byte `PDGOPFramebuffer`,
2560x1600, stride 10240, x2r10g10b10), stores into the mapping, no `PDGOPPresent` (that call is
for the VirtIO service only). The mapping is read exactly once, to save the console image; the
image is put back after every present run and at exit, and the program prints only while it is
back, as `wm-darwin`'s benchmark does. `SIGINT`/`SIGTERM` stop the run and still restore. If
PDGOP does not open, or is not a directly mapped x2r10g10b10 framebuffer, the present step
writes to a heap buffer of the same layout and says `sink=heap`.

| Scene | Content |
|---|---|
| `plasma` | one full-screen triangle, procedural fragment shader (four `sin`, one `sqrt` per pixel) |
| `quads` | 10000 textured, alpha-blended quads from a 256x256 atlas, one indexed draw call; about 17 layers of overdraw at 2560x1600 |
| `mesh` | nine tori (16384 triangles each), per-pixel diffuse + specular lighting, 24-bit depth buffer |

| Mode | One frame is |
|---|---|
| `render` | draw, `glFinish` |
| `readback` | draw, `glReadPixels` of the whole target |
| `present` | draw, `glReadPixels`, rows into the panel mapping |

| Path | Target and present step |
|---|---|
| `rgba8` | RGBA8 target in GL orientation. Present converts each pixel to x2r10g10b10 and reverses the row order on the CPU, a row at a time through a 10 KiB buffer (the compositor's shape of work) |
| `rgb10` | RGB10_A2 target; the shaders flip y and swap red and blue, so a `GL_RGBA`/`GL_UNSIGNED_INT_2_10_10_10_REV` readback is already the panel's word (`r<<20 \| g<<10 \| b`), top row first. Present is one 16 MB copy |
| `direct` | as `rgb10`, but `glReadPixels` writes straight into the panel mapping (present mode only): no second copy |

asahi supports the 10-bit path as far as the builder can tell: on the drm-shim the driver
reports `GL_IMPLEMENTATION_COLOR_READ_FORMAT/TYPE` = `GL_RGBA`/`GL_UNSIGNED_INT_2_10_10_10_REV`
for an RGB10_A2 target (`AIL_FMT(R10G10B10A2_UNORM, ...)` in `src/asahi/layout/formats.c`), and
all 21 runs complete without a GL error. The program asks at run time and drops the `rgb10` and
`direct` paths if the answer differs.

Each scene runs render/readback/present on `rgba8`, the same on `rgb10`, then present on
`direct`: 7 runs per scene, 21 in all. A run is `--frames` frames (default 30) after one
untimed warm-up frame, cut short after 2 s (never below 5 frames); `--budget` seconds (default
50) after start, remaining runs are skipped with an `AGXBENCH_SKIP` line. Frames depend on the
frame index only, never on the clock.

### Output

```
agx-panel-demo: GL_RENDERER=Apple M1 (G13G B1)
agx-panel-demo: GL_VERSION=OpenGL ES 3.2 Mesa 26.3.0-devel
agx-panel-demo: RGB10_A2 read format 0x1908 type 0x8368: 10-bit path on
agx-panel-demo: size=2560x1600 frames=30 budget_s=40 sink=panel stride=10240 setup_ms=...
AGXHASH scene=plasma target=rgba8 frame=7 size=2560x1600 sha256=<64 hex>
AGXHASH scene=plasma target=rgb10 frame=7 size=2560x1600 sha256=<64 hex>
AGXCHECK scene=plasma differ_px=0 of=4096000 max_diff=3 flat=0 result=ok
AGXBENCH scene=plasma mode=render frames=30 median_us=.. min_us=.. max_us=.. fps=.. path=rgba8 sink=none read_us=.. copy_us=0
AGXBENCH scene=plasma mode=readback frames=30 ... path=rgba8 sink=none read_us=.. copy_us=0
AGXBENCH scene=plasma mode=present frames=30 ... path=rgba8 sink=panel read_us=.. copy_us=..
AGXBENCH scene=plasma mode=render ... path=rgb10 ...
AGXBENCH scene=plasma mode=readback ... path=rgb10 ...
AGXBENCH scene=plasma mode=present ... path=rgb10 sink=panel ...
AGXBENCH scene=plasma mode=present ... path=direct sink=panel read_us=.. copy_us=0
  ... the same 10 lines for quads and for mesh ...
AGXBENCH_DONE status=ok skipped=0 seconds=..
```

The first eight fields of an `AGXBENCH` line are the requested ones; `path`, `sink`
(`panel`, `heap` or `none`), `read_us` (median time from the start of the frame to the end of
`glFinish`/`glReadPixels`) and `copy_us` (median of the CPU convert/copy that follows) are
appended. `fps` is 10^6 / `median_us`. Exit status 0 only for `status=ok`.

- `AGXHASH`: SHA-256 of frame 7 as read back, per scene and target. Comparable only between
  runs of the same driver at the same size (Linux asahi on this M1 against Darwin asahi).
- `AGXCHECK` needs no reference: it converts the `rgba8` frame exactly as the present step
  does and compares it with the `rgb10` frame, so the flip, the channel swap and the
  conversion are checked against each other; it fails if more than 2 % of pixels differ by
  more than 16/1023 in a channel, or if the frame is a single flat colour. On llvmpipe at
  640x400 the largest difference is 3, 13 and 3 (plasma, quads, mesh) and no pixel exceeds 16.

### Linux reference (for the lead, on the M1's Linux)

```
cc -O2 -o agx-panel-demo agx-panel-demo.c $(pkg-config --cflags --libs egl glesv2) -lm
./agx-panel-demo --require-renderer "Apple M1" --frames 30 | tee agx-panel-linux.txt
./agx-panel-demo --require-renderer "Apple M1" --frames 30 --no-present    # without the heap present runs
```

There is no panel access in the Linux build: `present` runs write to a heap buffer
(`sink=heap`), which measures readback plus conversion without the cost of the panel mapping.
`--dump prefix` writes frame 7 of each scene as `prefix-<scene>.ppm` for a look at the picture.
The six `AGXHASH` lines are the reference for Darwin.

### On PureDarwin

Bundle: `nix-build $F agx-offscreen-test.nix --no-out-link` (store path in section 2); the four
libraries and `agx-offscreen-test` are byte-identical to the bundle that passed on hardware,
only `usr/local/bin/agx-panel-demo` is new. Staging list, with `T` that store path:

```
$T/usr/local/bin/agx-panel-demo=/usr/local/bin/agx-panel-demo
$T/usr/local/bin/agx-offscreen-test=/usr/local/bin/agx-offscreen-test
$T/usr/local/lib/mesa-asahi/libEGL.1.dylib=/usr/local/lib/mesa-asahi/libEGL.1.dylib
$T/usr/local/lib/mesa-asahi/libGLESv2.2.dylib=/usr/local/lib/mesa-asahi/libGLESv2.2.dylib
$T/usr/local/lib/mesa-asahi/libgallium-26.3.0-devel.dylib=/usr/local/lib/mesa-asahi/libgallium-26.3.0-devel.dylib
$T/usr/local/lib/mesa-asahi/libdrm.2.dylib=/usr/local/lib/mesa-asahi/libdrm.2.dylib
```

`tools/agx_boot.sh` builds its own `PCIE_EXTRA_FILES` from this recipe and lists only
`agx-offscreen-test` and the libraries; it needs the first line above added (that file is
outside this directory). Guest script: `supervisor/darwin-agx-panel-demo.in`
(`AGX_TEMPLATE=...`), prefix `DL42_`, runs
`agx-panel-demo --require-renderer "Apple M1" --frames 30 --budget 40 --hold-ms 250`
and prints `DL42_PANEL_RC=<rc> seconds=<n>`. `--hold-ms` leaves the last frame of each
present run on the panel for that long before the console comes back (9 runs, 2.25 s in all).
Expected wall time: at most 40 s of runs plus the run in progress (2 s cap plus its warm-up
frame), the holds and set-up; the 2 s cap bounds 21 runs to about 42 s even if every frame
were slow, so on a slow target the last runs are skipped rather than the script overrunning.

### What the demo adds to the kernel's workload (from the drm-shim trace, 5 frames per run)

Same request set as section 4 plus `DRM_IOCTL_ASAHI_GEM_MADVISE` (once, from the BO cache).
Objects of 16 MB (`0xfa0000`, `0xfe0000`) are created and mapped. Render submissions carry two
attachments (`SET_FRAGMENT_ATTACHMENTS:48`, colour and depth); every `glReadPixels` is one
`COMPUTE` submission; every `glFinish` is the sync-file export/import/wait sequence. A full
default run is about 1100 submissions (651 render, about 470 compute) in under a minute.

### Risks of the demo

1. **The panel is left showing a frame if the process dies.** A crash or `SIGKILL` between a
   present and the restore leaves the last GPU frame up; the text console repaints as it
   scrolls. The serial log is unaffected.
2. **Static IOKit next to libIOKitCF.** The demo links PDGOP's `libIOKit.a` members into the
   executable while `libdrm.2.dylib` (loaded through `libEGL`) depends on
   `/usr/lib/libIOKitCF.dylib`, which exports functions of the same names with other
   signatures. Two-level namespace binding keeps them apart, and libdrm's `/dev/dri` path makes
   no IOKit calls; this combination has not run.
3. **Direct readback into a write-combined mapping** relies on Mesa only writing to the
   destination. It is the path most likely to be slow or wrong on the target; it is last in
   each scene, and the other two present paths do not depend on it.
4. **Readback cost is unknown.** On Linux `glReadPixels` of 16 MB from this driver is a
   compute job plus a CPU copy out of a GPU buffer mapping; if PureDarwin maps that buffer
   uncached or write-combined the copy is far slower than on Linux. `read_us` minus the
   `render` median shows it.
5. **GLES3 and 16 MB objects are new on the target**: the hardware-proven test used a GLES2
   context and a 256x256 target.
6. **Timer resolution**: libSystem's `CLOCK_MONOTONIC` is derived from `gettimeofday`
   (microseconds, and it moves if wall time is set during the run).

## 7. Using the GPU from the compositor (on paper)

State today: `wm-darwin` is Smithay's Wayland front end with a tiny-skia CPU renderer, `wl_shm`
clients only, PDGOP stores as the only output. Direct scanout is out of scope until the display
controller driver (SF-H); every option below ends in a readback and a copy to the panel.

**Option 1: GL renderer in the compositor, FBO plus readback (what section 6 measures).**

- Mesa: nothing new. Surfaceless EGL, GLES2/3 and asahi are in the bundle.
- Kernel: nothing new beyond what the demo already uses.
- Compositor: a second implementation of its renderer trait on GLES. Client buffers stay
  `wl_shm` and are uploaded with `glTexSubImage2D` for damaged regions; the scene is drawn
  into an RGB10_A2 FBO with the flip and channel swap of the `rgb10` path; output is
  `glReadPixels` of the damaged rows into the PDGOP mapping. Smithay's GLES renderer expects
  an EGL display from a native display type; a surfaceless one has to be supplied (not
  checked against Smithay 0.7's API).
- Cost per full frame is the demo's `present` line for `rgb10` or `direct`; with damage
  tracking only the damaged rows are read back. If that line is not clearly below the CPU
  compositor's 24 ms write plus 9 ms convert, this option only pays for scenes the CPU cannot
  draw in time (blur, scaling, many surfaces), not for a terminal.
- Size: 2 to 3 weeks (ticket G6). No dependency on G3/G4.

**Option 2: zero-copy clients (GPU clients hand dma-bufs to the compositor).** Builds on
option 1; it removes the client-side readback and the `wl_shm` upload, not the final copy.

| Piece | State | Work |
|---|---|---|
| Mesa GBM (`-Dgbm=enabled`) | builds and links for Darwin with the five patches here and nothing else (`libgbm.1.dylib`, `dri_gbm.dylib`; tried in a scratch tree, not in a recipe) | a recipe flag; where `libgbm` looks for its backend at run time is untested |
| Mesa EGL Wayland platform for clients (`-Dplatforms=wayland`) | not attempted; needs the flake's `wayland-arm64`, `wayland-protocols`, a host `wayland-scanner`, and the `ppoll` shim PureDarwin's `mesa.nix` adds for `loader_wayland_helper.c` | 2 to 3 days |
| `EGL_EXT_image_dma_buf_import` (+ `_modifiers`) in the compositor's EGL | Mesa enables it when the kernel answers `DRM_CAP_PRIME` (patch 0004 makes Darwin ask) and the driver can import; asahi can | check the extension string on the target |
| Compositor protocol: `linux-dmabuf` v4 with feedback (Smithay has the state objects); legacy `wl_drm` is not needed by current Mesa clients | missing | about 1 week with the import path |
| Kernel: PRIME handle to fd and fd to handle across two DRM files | DRM core `drm_prime.c` + `pd_dma_buf.c`; unexercised by asahi so far (0 PRIME calls in both traces) | test first |
| Kernel: dma-buf fds over a Unix socket (`SCM_RIGHTS`) | missing: the file type is not sendable (ticket G3) | 1 week with the item below |
| Kernel: `poll`/kqueue on sync files and DRM fds | missing or level-only (G3) | Wayland clients block on these |
| Kernel: `DMA_BUF_IOCTL_EXPORT_SYNC_FILE` / `IMPORT_SYNC_FILE` | missing (G4). asahi calls both for every shared buffer on every submission; when they fail it falls back to waiting on the CPU, so this is a performance item, not a correctness one | 1 week |
| Kernel: dma-buf `mmap` | missing (G4); the GL import path does not need it, CPU fallbacks and screenshots do | with the above |
| Device identity for clients | `linux-dmabuf` feedback sends a `dev_t`; open DRM nodes here are clones with a different minor than the node. libdrm's patch compares the low byte, Mesa calls `drmGetDeviceFromDevId` | test with the first client |

Order: run section 6 and read the numbers; option 1; then G3 (fd passing and wake-ups), the
Mesa GBM/Wayland build and `linux-dmabuf` in the compositor, with G4 for speed; Vulkan and
scanout later. Option 2 is about 4 to 5 weeks on top of option 1, most of it kernel file
plumbing that does not need the GPU driver.

## Files

```
common.nix                     flake pin, Mesa source pin
mesa-native.nix                build-machine tools, Linux asahi + drm-shim
mesa-darwin.nix                arm64 Darwin Mesa (asahi, softpipe, surfaceless EGL, GLES2/3)
agx-offscreen-test.nix         arm64 Darwin bundle: both programs and the libraries
agx-offscreen-test-linux.nix   builder-side builds of both programs, tracer, run-shim, run-sw
patches/                       5 source patches
compat/sys/sysmacros.h         include shim
test/agx-offscreen-test.c      the test
test/agx-panel-demo.c          on-screen demo and benchmark
supervisor/                    guest scripts (darwin-agx-panel-demo.in is the demo's)
trace/ioctl-trace.c            LD_PRELOAD ioctl/mmap logger
trace/summarize.py             condenses a log
trace/shim-g13g-trace.txt      recorded log
trace/shim-g13g-summary.txt    its condensed form
```
