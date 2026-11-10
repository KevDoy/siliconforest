# m1-dcp: Omacom's display-controller (DCP) driver as a kext (SF-H)

Status: **compile probe only.** No object file, no kext, nothing loaded or booted.

Source of truth: `sources/gpu-plan/linux-aurora` (`integration/aurora` at 6a08a08e),
`drivers/gpu/drm/apple`. The driver's sources are used unmodified; there are no patches
against the pin yet.

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
