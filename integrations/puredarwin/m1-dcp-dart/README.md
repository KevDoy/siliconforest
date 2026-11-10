# m1-dcp-dart: the display DARTs behind Linux's IOMMU/DMA API

For the pin's `drivers/gpu/drm/apple` (omacom/linux-aurora `6a08a08e`) on J293/T8103: the
devices behind `/arm-io/dart-dcp` and `/arm-io/dart-disp0`, served through the calls the
driver makes. Grown out of `../m1-gpu-rtkit/probe/pd_dcp_dart.cpp`, which ran on the M1
(boot dc11). **This directory is host-tested (table code) and compile evidence otherwise;
it has not run on hardware.**

| Path | What |
| --- | --- |
| `include/pd_dcp_iommu.h`, `src/pd_dcp_iommu.c` | LinuxKPI form: `iommu_*`, `dma_*`, `of_dma_configure`. Force-include the header for every file that does DMA for a display device |
| `include/pd_dart.h`, `src/pd_dart.cpp` | one stream of a firmware-provisioned T8020 DART (IOKit) |
| `include/pd_dart_table.h`, `src/pd_dart_table.c` | the page-table code, no kernel dependency |
| `test/table_test.c`, `Makefile` | host test: `make test` |
| `build-probe-kc.nix` | one-boot collection: `PDRtkitProbe` (mode 5) + `PDGpuDT` (display tree summary) |

## Streams

| Device (Linux node) | DART, stream | State found (boots dc07, dc11) |
| --- | --- | --- |
| `dcp@231c00000` (`iommus = <&dcp_dart 0 ...>`, `t8103.dtsi:663`) | `/arm-io/dart-dcp`, 0 | CONFIG 0x8001e100 (locked), TCR 0x80, root 0xbfff0c000, 4 first-level entries |
| `display-subsystem` (`<&disp0_dart 0 ...>`, `:704`) | `/arm-io/dart-disp0`, 0 | CONFIG 0x80016100 (not locked), TCR 0x80, root 0xbfff34000 plus three roots m1n1 added |
| `dcp/piodma` (`<&disp0_dart 4 ...>`, `:685`) | `/arm-io/dart-disp0`, 4 | TCR 0x80, root 0xbfff04000; TTBR 1..3 hold no valid root |

Linux sees both DARTs locked: m1n1 sets the lock on `dart-disp0` before it starts Linux
(`kboot.c:5247`), and `disp0_dart` is `status = "disabled"` in the tree until m1n1 enables it
(`t8103.dtsi:632`, `kboot.c:2440-2452`) precisely so that it is never reset. Under this lab's
hypervisor boot that lock is not set. The code here treats every stream as locked regardless:
TCR, TTBR, CONFIG and the stream enables are never written.

What Linux does with a locked T8103 root (`apple-dart.c:1053-1070`) is replace all of its
entries with Linux's own tables, into which the IOMMU core has first re-created the firmware's
mappings from the reserved-memory nodes m1n1 wrote (`kboot.c:2332-2470`: `dcp_data`,
`region57`, `region94`, `region95` and the framebuffer, each with `iommu-addresses`). Here the
firmware's entries are simply left in place (m1n1's own way, `dart.c:726-783`): nothing that
reads nonzero is ever written, so iBoot's framebuffer mapping in `dart-disp0` cannot change.
The observable difference from Linux: a mapping at an address inside a first-level slot the
firmware already populated lands in the firmware's table (in an empty leaf), not in a copy.

Addresses: allocations come from first-level slots 8..71 (iova 0x10000000..0x90000000),
skipping any slot that is not empty when the stream is opened; Linux's allocator works down
from the top of the same 0xfc000000 window. Addresses are handed out as plain stream
addresses, as Linux does; `pd_dcp_iommu_dva_mask = 1` gives m1n1's form
(`| asc-dram-mask`, 0xf00000000), which is what boot dc11 used. An address the firmware
supplies is masked to 32 bits before the lookup (`apple-dart.c:1728-1746`).

## Interface for the driver kext

Force-include `include/pd_dcp_iommu.h` (after the layer's headers are reachable; it includes
what it needs). It redirects, for bound devices only, and passes every other device through
to the layer:

- `iommu_get_domain_for_dev`, `iommu_iova_to_phys`, `iommu_map`, `iommu_unmap`,
  `iommu_map_sgtable`, `iommu_is_dma_domain`, `iommu_device_use_default_domain`,
  `iommu_device_unuse_default_domain`, `iommu_group_get`/`_put`/`_for_each_dev` (no groups:
  NULL / no-op; only the H17P paths ask), `struct iommu_domain` with `pgsize_bitmap`,
  `IOMMU_READ`/`WRITE`/`CACHE`;
- `of_dma_configure` (binds from the node's `iommus`);
- `dma_set_mask_and_coherent`, `dma_alloc_coherent`/`dma_free_coherent`,
  `dma_alloc_attrs`/`dma_free_attrs`, `dma_alloc_wc`/`dma_free_wc`, `dma_get_sgtable`(`_attrs`),
  `dma_map_resource`/`dma_unmap_resource`, `dma_mapping_error`, `dma_map_sgtable`/
  `dma_unmap_sgtable`.

`memremap` of a firmware buffer is the layer's own (`dcp.c:846`); nothing is needed from here.

Binding is automatic for a device whose `of_node` has `iommus = <&dart sid ...>` and whose
DART node has `reg` (the nodes `../m1-gpu-dt` publishes with `pddispdt=2` do), or explicit:
`pd_dcp_iommu_bind(dev, "/arm-io/dart-dcp", 0)`.

`dma_alloc_coherent` allocates single pages, maps them at consecutive addresses and returns
one kernel view (`vmap`), as iommu-dma does, so a 16 MiB framebuffer does not need 16 MiB of
contiguous memory, with one TLB invalidate per call.

User mappings: `/dev/dri` mmap asks the DRM kext for pages (`pd_drm_mmap_pages`), so
`dma_mmap_wc`/`dma_mmap_pages` are not reached; `pd_dma_buffer_pages(dev, cpu_addr, size,
pages)` gives that resolver the physical pages of a buffer from `dma_alloc_*` (see
`ANSWERS.md` for the resolver).

Not provided: IOMMU fault reporting (`pd_dcp_iommu_report` reads the error register on
request).

`ANSWERS.md` answers the PDDcp owner's questions (binding of `piodma`, the published nubs,
`of_find_compatible_node` iteration, `status`).

## Writes

Per stream: first-level entries of the live root, only in slots that read 0; leaf entries,
only where the slot reads 0; `STREAM_SELECT`/`STREAM_COMMAND` for the TLB invalidate. Never:
TCR, TTBR, CONFIG, enables, the error register, any nonzero entry.

## Test

`make test` (host, needs only `cc`): the walk with boot dc11's numbers, mapping into an
empty slot (the leaf value dc11 wrote), refusal over firmware and own entries with rollback,
non-valid-but-nonzero slots, unreachable tables, table exhaustion, ranges across slots,
search by physical range, unmap.

## One-boot check

```sh
cd integrations/puredarwin/m1-dcp-dart
$HOME/.nix-profile/bin/nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-probe-kc.nix
# boot-args: pdrtkitprobe=5 pdgpudt=1 pdgpudt_power=0 pddispdt=1
```

`PDRtkitProbe` mode 5 (see `../m1-gpu-rtkit/README.md`): read-only look at `dart-disp0`
streams 0 and 4 with the boot framebuffer searched for in stream 0; the DCP wake of mode 4
with the pin's `dcp_rtk_shmem_setup` code on this API; then one pass over
`dma_alloc_coherent` (5 pages), `dma_get_sgtable`, `iommu_iova_to_phys`, `dma_free_coherent`,
`dma_map_resource`/`dma_unmap_resource` on the dcp stream. `dart-disp0` is not written.

## Added by the PDDcp owner: mapping another driver's pages

`pd_dma_map_phys_pages(dev, phys[], n, &dma)` / `pd_dma_unmap_phys_pages(dev, dma, n)`
(`src/pd_dcp_iommu.c`, `include/pd_dcp_iommu.h`): `n` physical pages, in order, at one
contiguous address of the device's DART stream, with one invalidate; nothing is allocated or
copied and the pages stay owned (and pinned) by whoever passed them. PDDcp uses it to scan out
a buffer exported by the GPU driver (`../m1-dcp/kext/pd_dcp_prime.c`). On the M1 (boots pr02,
km01) every page of each imported buffer translated back correctly through the tables. The
host tests in `test/` do not cover this pair.
