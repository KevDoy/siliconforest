# m1-dcp: Omacom's display-controller (DCP) driver as a kext (SF-H)

Status: see `reports/sprint-010/` for what has run on the M1 (the coordinator records boots;
this directory holds recipes, sources and tests).

Source of truth: `sources/gpu-plan/linux-aurora` (`integration/aurora` at 6a08a08e),
`drivers/gpu/drm/apple`. Two patches against the pin (`patches/`, 9 changed lines): one
identifier that clashes with a LinuxKPI macro, and three stage-log lines.

## What is here

| Path | What |
| --- | --- |
| `include/pd_dcp_compat.h` | Force-included in front of every driver file: names Linux 7.2 has and the layer lacks or spells differently. Each entry says which |
| `tools/pin-headers.txt` | 30 headers the driver needs that the layer lacks or has as empty placeholders; taken from the pin's `include/` as they are at build time, not copied here |
| `tools/probe.sh <workdir> [file ...]` | Syntax-checks the 29 module sources against the layer with the layer's own C flags; prints errors per file |

## Measurement (M-a)

`tools/probe.sh` over the 29 objects of `appledrm` without audio and tests:

| Step | Errors |
| --- | --- |
| layer as it is (every file stops at its first missing header) | 40 |
| missing headers present but empty | 429 |
| the pin's own headers | 325 |
| `drm_atomic_commit` = `drm_atomic_state`, workqueue/KUnit/flag names | 145 |
| `scoped_guard` label fix (layer patch 0016) | 113 |
| device-tree iteration macros, `DECLARE_COMPLETION_ONSTACK`, `alloc_ordered_workqueue` | 58 |

20 of 29 files have no errors, among them `afk.c`, `parser.c`, `iomfb.c`, `systemep.c`,
`ibootep.c`, `iomfb_v12_3.c`, `iomfb_v13_3.c`, `iomfb_v14_7*.c`, `iomfb_h17p.c`, `connector.c`,
`dcp-hdmi.c`, `dptxep.c`, `apple_resume.c`.

The 58 that remain, by file:

| File | Errors | What is missing |
| --- | --- | --- |
| `dcp.c` | 5 | `iommu_domain.pgsize_bitmap` (2), `SYSTEM_SLEEP_PM_OPS`, `MODULE_DEVICE_TABLE(of, ...)`, `platform_driver.driver_managed_dma` |
| `apple_drv.c` | 12 | `for_each_*` device-tree loops not yet covered, `drm_gem_fb_create`, `file_operations.get_unmapped_area`, module macros |
| `plane.c` | 9 | `DRM_FORMAT_P230`, `DRM_FORMAT_MOD_APPLE_INTERCHANGE` (the pin's `drm_fourcc.h`) |
| `dcp_backlight.c` | 7 | `backlight_ops.controls_device`, `backlight_properties.state`, `BACKLIGHT_PLATFORM`, `BACKLIGHT_SCALE_LINEAR`, `BL_CORE_SUSPENDED` |
| `iomfb_v26_6.c` | 2 | the same two backlight constants |
| `dcpext_drm.c` | 5 | shadow-plane helpers (`drm_gem_atomic_helper`), `drm_gem_fb_create_with_dirty` |
| `dcpext_scanout.c` | 4 | `pgsize_bitmap`, `struct reserved_mem`, `dma-map-ops.h` clash |
| `j613_dcp_activate.c` | 13 | `struct of_changeset`, `uuid_t`, a scoped loop |
| `dcp-fabric.c` | 1 | `struct of_changeset` |

Syntax checking stops short of code generation and linking: the list of functions the layer
must provide (component framework, `of_graph`, GEM DMA and FB DMA helpers, atomic helpers at
7.2, backlight class, iommu) comes with M-b.

## Decisions so far

- `struct drm_atomic_commit` (7.2) is `struct drm_atomic_state` (the layer's DRM core, 6.14)
  under another name: one `#define` in the compat header, and no error names a field or
  function of it afterwards. Whether the atomic helper *functions* the driver calls exist
  with 7.2 signatures is a link-time question.
- Headers for subsystems J293's internal panel cannot reach (mux, phy, Type-C, GPIO, syscon)
  are the pin's declarations; their functions will be stubs, each listed with the call site
  that shows it unreachable, once the link shows which are referenced.

## Needed from other directories

- `m1-dcp-dart`: `struct iommu_domain` with `pgsize_bitmap`, `IOMMU_READ`/`IOMMU_WRITE`,
  and the `iommu_*` functions `dcp.c` and `dcpext_scanout.c` call.
- `m1-gpu-dt` / the layer's `of`: `of_find_compatible_node`, `of_get_next_available_child`,
  `of_prop_next_u32`, `of_graph_*`, `struct of_changeset` (only `j613`/fabric use it),
  `struct reserved_mem`.
- Layer (`m1-rust-kext/lkpi/patches-layer`, 0016 onward): `scoped_guard` label fix (0016,
  done); the device-tree loop macros, `DECLARE_COMPLETION_ONSTACK` and the 7.2 names now in
  `pd_dcp_compat.h` belong in the layer's headers once the kext builds.

## Kext and collection (M-b, M-c)

```
nix-build build-kext.nix     PDDcp.kext + PDDRMDri.kext ($out/nix-support: undefined, PDDcp.map)
nix-build build-kc.nix       bridge collection + PDGpuDT + PDDRMDri + PDDcp (no Wi-Fi, no PDAgx)
tools/regen.sh setup|split   regenerate patches/ from an edited copy of the driver
```

Built into the kext: 27 driver files as at the pin (+ the two patches); from the pin as they
are `drm_gem_dma_helper.c`, `drm_fb_dma_helper.c`, `drm_gem_framebuffer_helper.c`,
`drm_simple_kms_helper.c`, `drivers/base/component.c`; the layer; the DRM core (m1-drm72);
m1-gpu-rtkit's RTKit port; m1-dcp-dart. PDDcp imports no symbol that PDAgx does not also
import, and the collection reports no unresolved symbol beyond the base collection's.
`--arg agx true` (PDAgx in the same collection) does not fit: 0x64000 bytes over.

Not built, with the reason J293 does not reach them (`kext/pd_dcp_linux.c`, part 3, `PD_STUB`):

| File | Entry points stubbed | Why unreachable here |
| --- | --- | --- |
| `dcpext_scanout.c`, `dcpext_drm.c` | `dcpext_scanout_terminal/requested/invalidate/link_restored/register/fault` | external-display scanout; callers test `dcp->external` or are firmware 14.7 paths; J293 has one DCP, `apple,dcp`, internal |
| `j613_dcp_activate.c` | `apple_j613_25g83_coldplug`, `apple_t8122_25g83_board`, `apple_j613_25g83_clock_hz` | J613/T8122 with firmware 25G83 only |
| `audio.c`, `av.c` | none | `CONFIG_DRM_APPLE_AUDIO` off |

Reduced or refusing functions supplied by the kext: device links (recorded, not acted on),
devres groups (no rollback of a failed component bind), `disable_work`/`enable_work`
(cancel only), `drm_mode_size_dumb` (pitch = width x bytes per pixel), `of_changeset_*`
(refuse), `of_find_node_by_path` (NULL), `drm_gem_dma_mmap`'s `dma_mmap_*` (never called:
user mappings go through PDDRMDri), system sleep callbacks (compiled, not wired).

### Boot (not yet done)

Boot-args: `pdgpudt=2 pdgpudt_stringprops=1 pddispdt=2 pddcp=1`, optionally `pddcp_debug=1`
(the driver's `dev_dbg` lines and every repeat of a callback) and `pdrtkit_debug=1`.

`pddcp` values: 0 (default) not attached; 1 attaches, binds and starts the coprocessor, and
refuses every atomic commit with -EPERM; 2 also lets clients set modes and flip.

Lines to expect, all prefixed `PDDcp:` (driver messages are `PDDcp: <device>: ...`):
`LinuxKPI runtime up, pddcp=1`; `dcp <node>: mailbox 0 is /arm-io/dcp (0), probing`;
`rtkit:` lines of the RTKit port; `device ... created for node piodma`;
`display-subsystem <node>: probing`; `endpoint 0x.. start requested (0)` per AFK endpoint;
`dcp_start: firmware compat id N, external 0`; `callback D... (in N, out M) context C:
answered` or `NO HANDLER` once per distinct callback. Which endpoints and callbacks appear,
and in what order, is not predicted here: this driver has not run on this layer.

Open before a boot is meaningful: the questions in `NEEDS-FROM-DART.md` (piodma device,
node matching, `of_find_compatible_node` over published nodes).

## Tests and other pieces added after the first bind

| Path | What |
| --- | --- |
| `test/dcp-flip-test.c`, `dcp-flip-test.nix` | KMS through the card node whose driver is `apple`: modeset, page flips with flip-complete events, `--wait read|poll|select|kqueue`, `--prime-from auto` (buffers of the GPU driver imported for scanout) |
| `supervisor/darwin-dcp-{flip,prime,wait}-test.in` | guest scripts (DL43, DL44, DL45 lines); `tools/dcp_boot.sh` stages them with `DCP_TEST=1|prime|wait` |
| `kext/pd_dcp_mmap.c` | user mmap of dumb buffers (PDDRMDri resolver) and the 3-argument `fb_create` adapter |
| `kext/pd_dcp_prime.c` | import of a dma-buf exported by another DRM kext, for scanout without a copy |
| `tools/symbolize.sh` | names kext addresses from a panic, from the link map kept with each build |
| `kernel-patches/` | changes that need a kernel rebuild and are applied nowhere here |
| `build-kc.nix --arg agx true` | PDAgx and PDDcp in one collection |
