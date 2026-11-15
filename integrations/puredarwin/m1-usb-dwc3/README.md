# Apple DWC3 / xHCI port (SF-AF milestone 1, in progress)

`build-xhci.nix` builds an experimental replacement RavynXHCIPort bundle
against the PureDarwin ARM64 SDK. It is not installed or in the daily kernel.
`build-dma.nix` builds the DMA adapter alone. Neither has performed controller
DMA on the M1; the installed hardware sampler belongs to `../m1-typec`.

`patches/apple-dma-lifetime.patch` retains upstream's MIT notices and binds a
prepared `pd,apple-dwc3-host` IOPlatformDevice. Its parent must initialize the
ATC PHY, DWC3 core and paired DART before publication; its
`PDAppleUSBDart` platform function supplies the kernel-lifetime DMA domain.
The xHCI driver validates DWC_usb31 identity and host role before attaching.
The same bundle keeps the existing PCI/A733/Tegra paths.

Every xHCI allocation and buffer address (DCBAA, scratchpad, context, command,
event, EP0, HID, bulk, isochronous and storage bounce buffers) uses the domain
on Apple. CPU physical addresses remain distinct from device addresses.
MSC direct client mapping is rejected by the existing noncoherent path, so
payloads bounce. Both physical and IOVA alignment honor 64KiB TRB boundaries.
`tools/mk_xhci_patch.py` regenerates the patch from the reference SDK tree and
asserts each expected replacement; it does not modify that checkout.

The USB1 domain shares one table root and allocator between ADT `dart-usb1`
reg 0 / SID 0 and reg 1 / SID 1, matching Linux `t8103.dtsi`. It references
the existing DART table implementation at build time. TLB failure poisons
the domain and quarantines backing; tables last for the kernel. `prepare()`
requires the controller held in reset and no concurrent DMA producer.

Removal closes admission, disables interrupts, wakes and drains synchronous
calls and retained hotplug/bulk workers, then requires successful xHCI halt
and reset. `PDAppleUSBDrain` returns busy if that fails; the parent must keep
the PHY, controller and DMA alive then. `pd-usb-dma-live` on the provider also
covers failed attachment. Successful stop aborts queued transfers, terminates
children and breaks bus/storage ownership cycles. Old clients retain their
controller and receive offline; descriptors unmap before releasing pages.

`host-test/run.sh` exercises production admission under concurrent removal
(100 cycles, eight callers) with ASan/UBSan, and compares the production IOVA
search against 4,194,304 independent fragmentation/alignment expectations.
`make -C ../m1-dcp-dart test` covers the referenced page tables. These tests
and SDK compilation do not establish MMIO, cache coherency or hotplug on M1.

Still required: T8103 ATC/PIPE/reset and DWC3 lifecycle integration, collection
build and real direct USB enumeration/HID/read-only storage/DHCP, then ten
hotplug cycles. SuperSpeed lane calibration follows USB2. See SF-AF for the
verified baseline and shared-M1 install/recovery rules.
