# SF-V: rebase onto upstream's current development tip

## Goal
The lab builds from PureDarwin `e7aa00a6` (6 November 2026, the **pin**). Owner decision,
11 November: move to upstream's current tip, side by side. Everything that works on the pin keeps
working and stays the default until the rebased build is proven equal in QEMU and on the M1; the
owner switches the defaults.

- **Tip (fixed for this work):** `c713d8fb9b048a38e2242d67e29b9957ced8c495`, branch
  `upstream-xnu-12377`, committed 11 November 2026 21:21 -0500 ("ext4: extent index keys, cluster
  I/O, journal fixes"). corecrypto submodule `f577aafe` (pin: `ea8e6afe`).
- **Clone:** `sources/puredarwin-rebase` (this stream's own; branch `siliconforest-rebase` at the tip
  commit, submodule with its own objects). `sources/puredarwin-upstream-next` belongs to the Cocoa
  stream and is not touched. Upstream is not followed past this commit unless the owner says so.
- **Delta pin to tip:** 119 commits. Flake inputs `nixpkgs`, `kc-tools`, `iig-tools`, `xnu-loader`
  are the same revisions; the cross toolchain is rebuilt only because the SDK changed (no LLVM
  build: `arm64-cross-toolchain` is 14 small derivations).

## State
| Step | State |
|---|---|
| Source switch applied to the shared recipes | **done** (commit `510737cc2`): 76 recipes import `rebase/pd.nix`; all 79 recipes that named the pin instantiate to the same derivation before and after, as do the default collection (`rjlxj88c….drv` → `z66w83kk…`) and the Nix-built RAM root inputs. Three recipes keep the revision because their own text is a build input (`m1-keyboard/build-kext.nix`, `m1-network-default-route/build.nix`, `m1-apcie-bridge/root-build/build-private-policy-objects-001.nix`); found by the comparison, which first showed 6 differing derivations |
| Kernel at the tip (`rebase/kernel.nix`, DEVELOPMENT) | **builds**: `/nix/store/2g19j25q7c4hsa4lamvh4ffrjwj2460c-puredarwin-kernel-arm64-t8103-siliconforest-0.1`. The two ported patches compile |
| Collection at the tip (`rebase/build-kc.nix`) | **builds, with upstream's kext link rule** (`-text_exec`): `/nix/store/8yk7f7qnp7mcsm40a291ja1wh8dwyw5j-puredarwin-kc-arm64-0.1`, 34 277 246 bytes (pin: 33 851 054). Every kext compiled unchanged, the Rust GPU driver included. `kc_overlap.py`: 0 overlapping segments. Members: the pin's 60 minus `PDSun50iMMC` |
| Launcher's offline load check | **passes** with `rebase/launcher-tip-members.patch` (59 members) |
| libSystem at the tip (`rebase/build-libsystem.nix`, the three patches of `libsystem-default`) | **builds**: `/nix/store/69sdpjfj7aypp318yqldys9a7b7a60w8-puredarwin-libsystem-siliconforest-0.1` |
| Tip images, RAM root, QEMU, M1 | see the sections below as they are filled |

## The switch (deliverable 2)
`rebase/pd.nix` holds both sources (checkout + revision) and returns the flake for `SF_PD`
(unset or `pin`: the pin, with the very flake reference the recipes carry today; `tip`: the tip).
`convert_recipes.py --write` replaces the `builtins.getFlake "…rev=e7aa…"` expression in 79 live
recipes by `(import <path>/rebase/pd.nix).pd` and changes nothing else. It leaves alone: frozen
history (`revisions/`, `attempts/`, `implementation-revisions/`, `host-revision*/`,
`build-recipes/`) and the other streams' directories (`cocoa`, `qemu-dev`, `homebrew`,
`libsystem-default`, `m1-native-boot`), which keep naming the pin (12 recipes).

**Checked on a scratch copy of `integrations/puredarwin` (the real recipes are not converted
yet):** 15 entry points instantiate to the same derivation before and after, among them the
default collection (`m1-boot-speed/build-kc.nix` → `rjlxj88c…-puredarwin-kc-arm64-0.1.drv` →
`/nix/store/z66w83kk8gj61xv5ych34rz1zv18hk70-puredarwin-kc-arm64-0.1`) and every Nix-built file
`tools/gpu_desktop.sh` stages (`m1-mesa-asahi/agx-offscreen-test.nix`,
`m1-sound/sysaudio/build-sysaudio.nix -A all`, `m1-sound/build-pdplay.nix`,
`m1-bwfm/netd/build-pd-netd.nix`, `m1-userspace-diagnostics/build-libsystem-spawnattr.nix`), plus
`m1-smp/build-kc-desktop.nix`, `puredarwin-patches/build-kc.nix`, `m1-touchbar/build-pdtbtest.nix`,
`m1-storage/build-kext.nix`, `m1-dcp/dcp-flip-test.nix`, `m1-mesa-asahi/agx-wl-demo.nix`,
`m1-network-native-resolver/build-mdnsd.nix`, `chonkstep-darwin/build-pdgop.nix`,
`chonkstep-darwin/target-deps.nix`. Not yet compared one by one: the other converted recipes
(older collections and probes); that comparison runs when the conversion is applied, and a
difference stops the work.

With `SF_PD=tip` on the same scratch copy, 21 leaf recipes evaluate (all kexts of the default
collection, the patched platform expert, corecrypto, IOHIDFamily and framebuffer builds,
kc-tools, libSystem with the spawn fix, the audio server, pdplay, pd-netd, Mesa, pdtbtest, the
NVMe kext). Evaluation only: whether they compile at the tip is the next step.

Two things the switch must not do, and how that is kept:
- **Caches keyed on recipe text.** `m1-boot-speed/tools/bs_nix_path.py` (used by
  `gpu_desktop.sh` and the suite scripts) and `qemu-dev/qd_image.py`
  (`runs/qemu-dev/base/inputs.json`) remember store paths by recipe content, not by environment.
  A tip run through them would either get pin paths or leave tip paths for the next pin run.
  Rule: `SF_PD` is set only by the rebase stream's own wrappers, always with `BS_NIX_CACHE=0`
  (never reads or writes that cache), and never for `qd_image.py`. With the conversion,
  `bs_nix_path.py` also gets `SF_PD` added to its key when the variable is set (pin keys unchanged).
- **The pin collection chain is not made two-headed.** The pin's collection is 15 recipes that
  rewrite each other's `buildPhase` text and name six store paths outright (kernel, kernel source,
  platform kext, bridge, two keyboard bundles). The tip gets flat recipes under `rebase/`
  (`kernel.nix`, then `build-kc.nix`, `ramdisk`, `qemu`) that call the same converted leaf recipes
  and list members in the pin collection's order; the chain itself is only read.

After the conversion every `bs_nix_path.py` key changes once (recipe text is part of the key), so
each tool's first run re-evaluates (20 to 70 s per recipe) and gets the same paths as before.

## What the working-tree changes in the three checkouts are
`sources/puredarwin-m1-desktop` (21 files), `-m1-bringup` (19), `-baseline` (clean):
- 14 files under `src/Kernel/xnu`: the kernel half of `m1-bringup/candidate.patch`, and in
  `-m1-desktop` also `m1-display/console-resume.patch` (`video_console.c`, `pe_init.c`). Exactly
  this is the source snapshot every pin collection's kernel is built from
  (`/nix/store/fzwhq5qb…-source` in `m1-dcp/kernel.nix`, `/nix/store/274mmx79…-source` in
  `m1-display/console-return/build-kc.nix`: both compared file by file, identical to the
  `-m1-desktop` tree and different from the pin in these 14 files only).
- 5 files under `src/Kernel/Extensions/PDArmPlatformExpert`: exactly
  `m1-bringup-platform/platform.patch` applied (compared file by file). The builds do not read
  them: the recipes apply the patch file to the pinned source.
- `flake.nix`, `nix/m1-bringup.nix`: the `kernel-arm64-t8103-bringup` package the first kernels
  were built with. Not used by current builds (`m1-dcp/kernel.nix` overrides the flake's T8010
  kernel instead).

The checkouts are therefore not needed for the tip: the kernel delta is now a patch file
(`m1-bringup/kernel-patches-tip/0001`), and upstream has a T8103 kernel package to start from.

## Inventory: every patch and overlay on the PureDarwin tree, at the tip
Method: each series applied in recipe order with `git apply` to a worktree of the tip (and, as a
control, of the pin, where all of them apply). "Applies" is textual; nothing here is compiled yet.

### Kernel (xnu), in build order
| Patch | At the tip |
|---|---|
| bring-up base (working tree of `-m1-desktop`: `candidate.patch` kernel half + `console-resume.patch`) | **conflicts with upstream's T8103** in 3 of 14 files (`MakeInc.def.in`, `MASTER.arm64.t8103`, `board_config.h`) and overlaps in `caches_asm.s`; **ported**: `m1-bringup/kernel-patches-tip/0001` (merge decisions below) |
| `m1-dcp/kernel-patches/0001` pd_lfile sendable, `0002` select/poll/kevent, `0003` mmap | apply (in series; `0003` needs `0002`) |
| `m1-dcp/kernel-patches/0004` T8103 as DEVELOPMENT | applies on the ported base |
| `m1-smp/kernel-patches/0001` AppleARMSMP reset vector | applies |
| `m1-smp/kernel-patches/0002` cpu_init MIDR line uses kprintf | **upstream has it** (same change in `cpu.c`): dropped at the tip |
| `m1-smp/kernel-patches/0003` park offline cores, idle accounting, deep idle | one hunk (`board_config.h`) needed moving; **ported**: `m1-smp/kernel-patches-tip/0003` |
| `m1-quiet-boot/kernel-patches/0001`, `0002` | apply |
| `m1-boot-speed/kernel-patches/0001` registry-quiet wait | applies (same file upstream added log lines to) |

Merge with upstream's T8103 (commit `fda0135e`, made for darwin-vm's QEMU machine: EL2/VHE entry,
no AppleARMSMP, `PUREDARWIN_NO_KERNEL_INTEGRITY`):
- **Upstream's taken, ours dropped:** `T8103` in the supported-configuration and large-memory
  lists; the assembler register names `HID4`, `EHID4`, `CPU_OVRD`, `CPU_CFG`, `ACC_OVRD`,
  `ARM64_REG_HID4_DisDcMVAOps` (now in `apple_arm64_regs.h`, same encodings); upstream's EL2 entry
  in `start.s` stays (not taken at EL1 under the m1n1 hypervisor).
- **Ours kept:** the machine flags (`DARWIN_LAB_M1_BRINGUP`, not
  `PUREDARWIN_NO_KERNEL_INTEGRITY`: that one turns `USE_APPLEARMSMP` off, and the lab starts its
  cores through AppleARMSMP + `PDAppleBootCPUPMGR`); our `MASTER.arm64.t8103` (derived from
  `MASTER.arm64.MacOSX`; upstream's is an xsmall/embedded base without SysV IPC: a different kernel
  for user space); our cache helpers (`m1_cache_helpers.inc`), with upstream's four functions of the
  same names compiled out under our flag. Ours restores the HID4 value it found, masks interrupts
  while that value is live and returns on an empty range; upstream's
  `CleanPoC_DcacheRegion_Force_nopreempt` switches the VA operations off unconditionally when it
  returns, also inside a caller's batch.
- **Could be dropped later, after a test of its own:** the cache helpers (if upstream's are made
  nesting-safe or shown not to nest), `m1_progress_entry.inc` and the `M1_PROGRESS` breadcrumbs
  (bring-up aids).

### PDArmPlatformExpert
| Patch | At the tip |
|---|---|
| `m1-bringup-platform/platform.patch` | applies |
| `m1-smp/platform-patches/0001`..`0004` | apply |

Upstream changed the same kext (+514 lines). What reaches the M1: `publishSocNubs()` now also
publishes `/arm-io/ans` on Apple SoCs (see Risks 3); the model name comes from the device tree
(`hw.model` changes); PSCI is tried by SMC on an Apple SoC and cores are started directly through
PMGR only at EL2. None of upstream's CPU start code is equivalent to ours under the hypervisor
(EL1, AppleARMSMP), so nothing is dropped.

### corecrypto, libSystem, other libraries
| Patch | At the tip |
|---|---|
| `m1-smp/corecrypto-patches/0001` DRBG state size | applies to corecrypto `f577aafe`; **upstream has not fixed it** (the submodule changed one file, `cc_config.h`) |
| `m1-userspace-diagnostics/fix-spawnattr/posix-spawnattr-init-zero.patch` (the RAM root's libSystem) | applies; **not fixed upstream** (`posix_spawn.c` unchanged) |
| `m1-userspace-diagnostics/fix-root/vm-page-size-symbol.patch` | applies (not in the current RAM root) |
| `libsystem-default/patches/macos-abi.patch` (was `homebrew/patches/libsystem-homebrew.patch`; issue #46, stack probe) | applies; still needed (Homebrew stream's file, not changed here) |
| `homebrew/patches/security-copy-certificate-chain.patch` | applies (Homebrew stream) |
| `m1-wayland/pdgop-fail-closed.patch` (PDGOP, linked into the compositor) | applies |
| `m1-sound/sysaudio/coreaudiod-dev-backend.patch` | **fails, all 10 hunks**: upstream rewrote `coreaudiod.c` (1079 → 1721 lines: capture, input IOProcs, several devices). Needs a real port |

### Kexts built from the tree with a patch
| Patch | At the tip |
|---|---|
| `m1-display/framebuffer-30bit.patch`, `m1-display/console-return/console-return.patch`, `m1-wayland/iogop-vram-write-combine.patch` (IOGOPFramebuffer) | apply (the kext is unchanged upstream) |
| `m1-native-input/revisions/002/candidate.patch` (IOHIDFamily; the recipe also checks file hashes before and after) | applies; IOHIDFamily is unchanged upstream, so the hashes hold |
| `m1-rust-kext/lkpi/patches-layer/0001`..`0040` (Linux layer, `src/Kernel/fbsdkpi`) | all 40 apply |
| `m1-drm72/patches/puredarwin/0001`, `0002` | apply |
| `m1-bringup/kc-tools-arm64-text.patch` (kc-tools, a flake input) | same input revision; unchanged |

### Kexts added to the tree as directories (no patch; `cp` + `add_subdirectory`)
PDApcieT8103Bridge (+ DART pool/map, SMC), IO80211Family + PDBwfm, PDGpuDT, PDCpuPower,
PDAppleSound, PDAppleTouchBar, PDM1SPIHID (multitouch + `spihid-touchbar.patch` on our own
source), PDDcp/PDDRMDri/PDAgx (Linux layer, Rust), PDAppleNVMe (not in the default collection).
The families they build on are unchanged between pin and tip (IOPCIFamily, IO80211Family,
IONetworkingFamily, IOGraphicsFamily, IOAudioFamily, IOHIDFamily, IOStorageFamily: no diff), and
no kernel export was removed (`Unsupported.arm64.exports` +4, nothing else). All 12 recipes
evaluate at the tip. Open until built: see Risks 1.

### Exports for upstream (`puredarwin-patches/`)
`0001-m1-pcie-wifi-kexts.patch` and `0002-…userspace-and-registry.patch` fail in the list files
only (`src/Kernel/Extensions/CMakeLists.txt`, `src/Userspace/CMakeLists.txt`, `nix/lib/kexts.nix`).
They are generated (`tools/pcie_export_patch.sh`) and not on the default build path: regenerate
against the tip after the equivalence proof.

### QEMU-only (QEMU stream's files, read only)
| Patch | At the tip |
|---|---|
| `qemu-dev/pdhid-publish-keyboard.patch` | applies; **not fixed upstream** (IOHIDFamily unchanged) |
| `qemu-dev/virtio-net-name-interface.patch` | applies; **not fixed upstream** (IOVirtIONet unchanged) |
| `qemu-dev/loader-gop-mode.patch` (xnu-loader, a flake input) | same input revision |

### Cocoa (Cocoa stream's files, read only)
`0001` is in the tip already; `0002`, `0003` apply; `0004` (WindowServer), `0005` (AppKit) and
`0006` (Onyx2D) fail at the tip. That stream's port; reported to it through the owner.

### Not PureDarwin-tree patches (unaffected by the rebase as text)
Driver sources and their patches against Linux (`m1-dcp/patches`, `m1-rust-kext/agx/patches`,
`m1-rust-kext/lkpi/patches`, `m1-lkpi-extras/rust/patches`, `m1-drm72/patches/drm-kmod`), Mesa
(`m1-mesa-asahi/patches`), the compositor (`chonkstep-darwin`, `m1-touchbar/wm-darwin-*.patch`)
and the launcher patches (`quiet-boot.patch`, `launcher-fast-quiet.patch`, `host_progress-smp.patch`,
`vcpu-dump.patch`, …). The launcher has one check that the tip breaks: Risks 2.

## Did upstream fix what we found?
| Finding | Upstream at the tip |
|---|---|
| corecrypto DRBG state size overrun | no; our patch still applies and is still needed |
| `IOKitInitializeTime` waits (2 × 30 s) | partly, and not for us: `IORTC` is published at once when the platform model name is `Apple ARM`; with the device tree's model (`MacBookPro17,1` on the J293) that branch is not taken, and the registry-quiet wait is unchanged. Our boot-args patch stays |
| IOHIDFamily keyboard node (`/dev/usb_hid_kbd`) | no |
| virtio-net interface naming | no |
| `posix_spawnattr_init` leaves fields unset | no |
| cpu_init printing with interrupts off (our `m1-smp` 0002) | yes, same fix |

## What upstream changed that affects us
- **Kernel for kexts:** no export removed; public IOKit and libkern headers unchanged; our kexts
  need a rebuild, not an adaptation, as far as the source says.
- **Kext layout:** `cmake/kext.cmake` now links arm64 kexts with `-text_exec` (code in
  `__TEXT_EXEC`, as riscv did before). Every member of the collection changes shape (Risks 1).
- **Collection members:** `kc-arm64.nix` builds its member list from a table and has
  `omitKexts`. Gone: `PDSun50iMMC`. New in the base list: `PDTegraSDMMC`, `PDGadgetFamily`,
  `PDTegraUSB`, `PDTegraXUSB`, `PDTegraDisplay`, `PDFBSDem` (49 → 54 members).
- **Kernel behaviour:** about 25 new `PD-IOK …` lines on the console per boot
  (`IOStartIOKit.cpp`, `pe_init.c`); `consdebug_putc` no longer calls `PE_kputc` when it is the
  video console; `IODTResolveAddressCell` accepts ranges whose parent and child cell counts differ;
  kernel-to-user MIG replies over 1 KiB go on the heap (`-maxonstack 1024`); `setxattr` without
  doubleagentd returns `ENOTSUP` instead of `ENOATTR`; core dumps are logged in every
  configuration; a `pd,mirror` serial property and a RAM-log console exist (need device-tree
  properties we do not have).
- **Linux layer:** new `src/Kernel/fbsdkpi/xnu/pd_dri_cdev.cpp` (a `/dev/dri` clone device);
  our layer patches for DRI descriptors (0020, 0028) apply but meet new code (Risks 4).
- **libSystem and dyld for our binaries:** libSystem gained 119 exports (libproc, dirhelper) and a
  dirhelper hook; dyld gained an opt-in `DYLD_PD_MISSING_OK`. Nothing removed: the compositor
  (`wm-darwin-scale-desktop-build-009`), terminal (`terminal-application-build-008`), Mesa, the
  audio server and the Wi-Fi tools built on the pin are expected to run unchanged on the tip's
  libSystem. To be shown in QEMU first; no rebuild of the compositor is planned unless that fails.
- **Audio user space:** CoreAudio, AudioToolbox and coreaudiod were reworked upstream; our
  coreaudiod backend patch no longer applies.
- **Image and RAM root:** the RAM root is the ext4 root of the pin's `image-arm64-virt-minimal`
  (`m1-rootfs/extract-ramdisk.sh`, store path `f35jpffq…`), shrunk, plus
  `artifacts/m1-desktop-010/apcie-port-candidate-003/mapping.json` and the extras each tool adds.
  At the tip the image is 69 derivations to build (kernel, libSystem, zsh, toybox, launchd, IOKit,
  Foundation, X libraries for the userland package); partition offsets must be read again, not
  assumed. `tools/pcie_assemble.py` names the base image by path; `PCIE_STAGER` is the override.
- **Toolchain/SDK:** SDK rebuilt (MIG host changes); cross clang and ld are the same nixpkgs
  packages with new wrappers. The `clang-msl.patch` is for the guest-hosted clang, which we do not
  build.

## Risks
1. **Kext code moves to `__TEXT_EXEC`.** The pin's collection has less than one kext of room
   below the kernel's `__TEXT` (`kc_overlap.py`), `kc-tools` carries our arm64 `__TEXT` patch, and
   the launcher's `normal_image.py` (hash-pinned, not to be edited) parses the collection's
   segments. An extra segment per kext costs alignment padding in 16 KiB pages. If the collection
   does not fit or the offline load check refuses it: build the tip's kexts with the pin's link
   rule (a one-line tree patch reverting `-text_exec` for arm64 in `cmake/kext.cmake`), report it,
   and keep the new layout as a separate item.
2. **The launcher's member gate** (`host_progress.py`, `APCIE_NATIVE_MEMBERS`) requires
   `org.puredarwin.kext.PDSun50iMMC`, which no longer exists. The tip collection cannot pass it
   unchanged. Planned: a per-boot `PCIE_LAB_PATCH` that removes that one name from the list (and is
   combined with the quiet-boot patch for the boots that use both); nothing in the reset or return
   path is touched. The five new Tegra/gadget kexts and `PDFBSDem` are left out with `omitKexts`
   (hardware we do not have, and room).
3. **`/arm-io/ans` is published as a nub** by upstream's platform expert on Apple SoCs. No kext in
   the tip matches it and our NVMe kext is not in the default collection, so nothing should attach.
   The first M1 boot checks the registry for a driver on that nub before anything else is
   concluded; `pdnvme=` is never passed.
4. **DRI descriptors:** upstream's new `/dev/dri` clone device next to our select/poll/clone-slot
   patches. Textually clean; behaviour shown only by the GPU stage.
5. **Device-tree address resolution** changed in `IODTResolveAddressCell`. The first M1 boot
   compares the `IODeviceMemory` ranges of the nodes our drivers map (PCIe, DART, SMC, DCP, AGX,
   audio, SPI) with a pin boot's.
6. **Console volume and timing:** 25 more lines before the root mounts; the quiet boot and the
   7.5 s first-frame figure are measured again, not assumed.
7. **`PCIE_TIMEWAIT`** patches the kernel by instruction pattern; a recompiled kernel may not
   match. The default desktop does not use it (boot-args `pd_quiet_wait=0 pd_rtc_wait=0`).
8. **Sound user space at the tip.** For the equivalence proof the pin's audio server and
   frameworks (already built) run on the tip's kernel and libSystem; porting our backend onto
   upstream's new coreaudiod is a separate piece of work and is reported as not done.
9. **QEMU tools share state:** `qd_image.py` deletes every older template of the same kernel and
   variant when it makes a new one, and keeps its Nix answers in one file. A tip run through
   `tools/qemu_desktop.sh` with `QD_BASE_IMAGE` would evict the QEMU stream's pin template each
   time. See "Needed from the owner".
10. **Host load:** all tip builds are new (nothing of the tip is in the store). Order below builds
    the small set first; the image (69 derivations) is built once, `--max-jobs 2 --cores 3`.

## Order of work
1. Apply the conversion; compare every converted recipe's derivation before and after; add
   `SF_PD` to the `bs_nix_path.py` key. Stop on any difference.
2. Tip toolchain, `xnu-headers-arm64`, `kexts-arm64`, the kernel (`rebase/kernel.nix`, DEVELOPMENT
   and DEBUG). First compile errors of the ported patches show here.
3. Every kext of the default collection at the tip, then `rebase/build-kc.nix` (members and order
   of the pin collection, `omitKexts` for the six new ones), `kc_overlap.py`, the launcher's
   offline load check (no bench). Decide Risk 1 here.
4. Tip libSystem with the spawn fix, tip minimal image, RAM root from it
   (`rebase/tools/stage_ramdisk_rebase.py` via `PCIE_STAGER`), QEMU image (`rebase/qemu-image.nix`:
   the QEMU stream's two recipes and three patches, with the tip source).
5. QEMU: the tip kernel collection, kexts and root with the M1's binaries, autopilot subset that
   passes there today, against a pin run of the same command on the same day.
6. M1, one area per boot series (`rb01`, …), each next to its pin result: shell on 1 CPU; 8 CPUs,
   3 boots (this does not re-prove the corecrypto hang, 1 in 9 on the unpatched kext: a 20-boot
   run goes to the overnight batch); Wi-Fi scan/join/DHCP/ping; GPU + display + compositor
   autopilot (20 phases, as `ds09`); sound (`m1-sound/tools`, `kern.pd_speaker.hold_probe=1`);
   Touch Bar (`m1-touchbar/tools`); boot timeline (`bs_timeline.py`, first frame ≤ 7.5 s); the
   whole desktop as `tools/gpu_desktop.sh`. Always `PCIE_KC=<tip collection>`, `PCIE_CHAIN` off,
   120 to 600 s, never `pdnvme=`.
7. Equivalence table, default changes as a patch for the owner, what can be dropped.

## Needed from the owner
- **QEMU runs of the tip** need their own template and input cache. Either the QEMU stream adds an
  environment variable for `qd_image.py`'s output root (`OUT`, now fixed to `runs/qemu-dev`), or
  the rebase stream runs that stream's unmodified `qd_image.py`/`qd_run.py` from a wrapper of its
  own that points them at `runs/qemu-dev-rebase` (no file of theirs edited, nothing shared
  written). The second needs no one else; say if the first is preferred.
- **Cocoa stream:** its patches `0004`, `0005`, `0006` do not apply at the tip; `0001` is upstream.

## Open, not in this stream: our audio backend on upstream's new coreaudiod
Owner decision (11 November): the equivalence proof runs the pin-built audio server and
frameworks on the tip's kernel and libSystem; the port is a follow-up.

What upstream's coreaudiod at the tip is (`src/Userspace/coreaudiod/coreaudiod.c`, 1721 lines):
a server for IOAudioFamily engines only. It finds every `IOAudioEngine` in the registry, opens
its user client (status buffer, `performClientIO` trap, register/unregister client buffer),
runs one IO thread per engine that mixes the clients' shared rings one period ahead of the
engine's playback head, and since the pin has gained input streams (capture, input IOProcs),
several devices with stable ids by UID, default-device choice and per-device preferences, and
volume/mute through `IOAudioLevelControl`/`IOAudioToggleControl`. **It has no pluggable device
backend**: an engine is an IOKit service or nothing. Our patch added a second kind of engine to
the old single-device server (a table of `/dev/pdaudio` and `/dev/pdspeaker` character devices
with `kern.pd_sound`/`kern.pd_speaker` sysctls for volume, jack state and the speaker hold).

Two ways to follow, to be decided by the sound stream:
1. Port the `/dev` engines onto the new `struct engine` (they become two more devices beside
   any IOAudioEngine; the new multi-device, default-device and preference code then applies to
   them). The patch is a rewrite, about the size of the old one (640 lines).
2. Make PDAppleSound an IOAudioFamily driver (`IOAudioDevice`/`IOAudioEngine` with level and
   mute controls). Upstream's server then drives it unpatched, capture comes for free when the
   microphone path exists, and the lab carries no coreaudiod patch at all. IOAudioFamily is in
   the tree and unchanged between pin and tip. This is the direction upstream's design points to.

## What could be dropped once the tip is the default (so far)
`m1-smp/kernel-patches/0002`; the register-name block of the bring-up patch's `board_config.h`;
the three dirty checkouts (`sources/puredarwin-m1-desktop`, `-m1-bringup`, `-m1-firstboot`) as
build inputs, and with them the six store paths written into the collection chain; the T8010
override in `m1-dcp/kernel.nix` (upstream has a T8103 kernel package). Not droppable: everything
listed as "not fixed upstream" above.
