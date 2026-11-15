# Apple DWC3 / xHCI port (SF-AF milestone 1, in progress)

`build-xhci.nix` builds an experimental replacement RavynXHCIPort bundle
against the PureDarwin ARM64 SDK. USB03 is installed as an experimental
collection; the default collection recipe remains unchanged.
`build-dma.nix` builds the DMA adapter alone. Neither has performed controller
DMA on the M1; the installed port-1 sampler belongs to `../m1-typec`.

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

Hardware validation still requires direct USB enumeration/HID/read-only
storage/DHCP, then ten hotplug cycles. SuperSpeed lane calibration follows
USB2. See SF-AF for the
verified baseline and shared-M1 install/recovery rules.

`build-host.nix` adds the actual port-1 `PDAppleUSB` lifecycle personality to
this bundle; `build-kc.nix` replaces the original xHCI bundle in an isolated
collection and includes the Type-C snapshot provider. Boot arguments are
`pdtypec=1 pdusbhost=1`. The daily collection is unchanged. Source and ARM64
collection build are complete; this host collection is not installed yet.

The lifecycle consumes copied `port-state` snapshots. Failed I2C samples
preserve the session. It powers/configures only a direct host cable with no
DP/TBT/USB4 flag; the TS3-only state remains deferred. It resolves and checks
all port-1 windows, validates tunables before writes, establishes stopped
controller ownership, parks/reset/powers the PHY, prepares DART, initializes
DWC3, selects USB2 host after core reset and publishes the xHCI nub. Removal
requires the child's drain and synchronous termination before PHY shutdown.
A failed attachment that still owns DMA also blocks shutdown and retry.
DART faults stop further sessions. Domains/tables live for the kernel.

The DWC3 host-only port checks the DWC_usb31 identity and capability, clears
PHY suspend during initialization, performs bounded device-half soft reset
when needed, applies the global-control/2.00a clock-gating workaround and
Apple CIO settings, disables gadget event DMA, then sets host role/suspend.
Clock timing defaults are retained as in the T8103 Linux DT, which supplies
no reference/suspend clock overrides. Host tests cover all role/power-option
combinations, reset ordering, invalid identities and timeout refusal.

Remaining hardware work starts with a captured first boot and direct USB2
attachment, followed by enumeration, HID, read-only storage, DHCP and ten
hotplug cycles. Common tunable/MMIO correctness, DART DMA/cache behavior and
IOKit removal are not proved by register models or SDK compilation.

The newer native adapter selects USB3 when the immutable Type-C sample
reports USB3 and still defers every DP/TBT/USB4 cable. It copies and retains
the runtime lane bundle before PMGR/PHY writes, powers/calibrates the
correct orientation, initializes DWC3, then routes PIPE before publishing
xHCI. The qualified T8103 path skips host BIST, matching Linux's workaround.
A change in USB2/USB3 mode drains the old session. Stop parks PIPE only
after native controller drain and synchronous termination; failed handoff
completion keeps PHY ownership and blocks a new session. No calibration
values are shipped. This source SDK-builds but is not installed in USB03.
