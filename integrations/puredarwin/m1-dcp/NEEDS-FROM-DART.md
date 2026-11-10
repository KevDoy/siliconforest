# What PDDcp needs from m1-dcp-dart and the display device tree (m1-gpu-dt)

Written by the PDDcp owner for relay. Each item says what is stubbed locally meanwhile
(`kext/pd_dcp_linux.c`) so the link is not held up.

## Used as delivered (no change asked)

- `m1-dcp-dart/include/pd_dcp_iommu.h` force-included for every driver file and for the pin's
  `drm_gem_dma_helper.c`, `drm_fb_dma_helper.c`; `src/pd_dcp_iommu.c`, `src/pd_dart_table.c`,
  `src/pd_dart.cpp` built into the kext. `struct iommu_domain.pgsize_bitmap`, `IOMMU_READ/WRITE`
  and all `iommu_*`/`dma_*` names the driver calls resolved with it.
- Binding from the node's `iommus` property is relied on for `dcp`, `piodma` and the
  display-subsystem device. PDDcp calls `pd_dcp_iommu_bind()` nowhere.

## Questions / requests

1. **`piodma` device.** `dcp.c:3170` does `of_platform_device_create(node, NULL, dcp->dev)` for
   the `piodma` child and then DMA-maps through that device. Locally this creates a bare
   `platform_device` with `dev.of_node` set to the child and no `dma_priv`. Does
   `pd_dcp_iommu.c` bind such a device from its node on first use, or does it need
   `pd_linux_dev_dma_init()` (the layer's default DMA tags) to have run on it first?
2. **Node names the IOKit personality matches.** `PDDcp` matches `IONameMatch`
   `apple,dcp` and `apple,display-subsystem` on `IOPlatformDevice`. Confirm that the published
   nubs are `IOPlatformDevice`s matched by compatible in that way (as `gpu@206400000` is by
   `apple,agx-t8103`), and that `piodma` is NOT published as its own matchable nub (the driver
   creates it).
3. **`of_find_compatible_node(from, NULL, compat)` over the published nodes.** `apple_drv.c`
   walks the tree for `apple,dcp` nodes (`for_each_matching_node`) to build the component
   match; locally `of_find_matching_node` is a loop over that function. It must find the
   published `dcp` node starting from NULL and return NULL after it.
4. **Machine compatible.** `of_machine_is_compatible("apple,j700")` etc. are answered by the
   RTKit port's `pd_rtkit_of_machine_is_compatible`. Nothing asked unless the display tree
   wants to answer it itself.
5. **Not provided by the layer, stubbed, only reached on other machines:**
   `of_find_node_by_path` (`iomfb_v26_6.c:1035`, returns NULL),
   `of_translate_dma_region_checked` (`iomfb_v14_7.c:1849`, returns -ENODEV),
   `of_changeset_*` (`dcp-fabric.c:3614`, refuse). No action unless those machines matter.
6. **`status` properties.** `of_get_next_available_child` is `of_get_next_child` here (the
   layer does not read `status`). If the synthesized display nodes carry `status =
   "disabled"` children, say so.
