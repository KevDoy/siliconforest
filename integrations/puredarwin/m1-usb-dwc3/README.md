# Apple DWC3 / USB DMA port (SF-AF milestone 1, in progress)

`build-dma.nix` builds ARM64 SDK **objects only**. No host controller service
or kernel collection member is added here yet. None of this DMA domain has
run on the M1. The active hardware test belongs to `../m1-typec`.

The USB1 domain in `kernel/pd_usb_dart.cpp` shares one table root and allocator
between ADT `dart-usb1` reg 0 / SID 0 and reg 1 / SID 1, matching the two
`iommus` entries in Linux's `t8103.dtsi`. Its attach and invalidation sequence
comes from the existing Linux Wi-Fi DART port; its table implementation is
the existing `m1-dcp-dart` implementation, referenced at build time. It does
not reuse the Wi-Fi DART or modify its mappings.

`PDUSBDMABuffer` allocates whole, owned 16KiB pages and gives the host a
separate DMA address. xHCI's contexts, rings, scratchpads, EP0, interrupt,
bulk, isochronous and BOT payload buffers all need this allocation path;
`getPhysicalAddress()` is a CPU physical address, **not** this IOVA. A failed
TLB invalidate poisons the domain and retains backing rather than recycling
memory a bank may still translate. The domain's table allocations last for
the kernel; unloading is unsupported. `prepare()` requires the controller in
reset and no producer modifying mappings concurrently.

Still required before first host boot:

- connect every xHCI DMA allocation/address use to this adapter; initial
  payload transfers bounce too (no untranslated direct MSC IODMACommand);
- port the T8103 USB2-only ATC/PIPE/reset and Apple DWC3 core sequence;
- Type-C role/mux lifecycle, then SuperSpeed calibration and lane routing;
- xHCI teardown: the current upstream `quiesce()` sets the hotplug stop flag
  but does not join its thread, and `free()` can release state that thread
  still uses. Stopping the xHC alone does not make whole-controller removal
  safe. Drain hotplug, bulk, interrupt producers and children before freeing;
- paired-DART SDK and host tests, then actual controller DMA and fault checks.

Source table tests: `make -C ../m1-dcp-dart test` (host only). The new MMIO
adapter and buffer lifetime are not established by those table tests. See
SF-AF for baseline, install/rollback and shared-M1 rules. No new boot argument
enables this directory's code yet.
