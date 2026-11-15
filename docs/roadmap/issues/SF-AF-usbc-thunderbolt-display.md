# SF-AF: USB-C, Thunderbolt and external display on J293

Owner: CODEX-TB. Survey started 15 November 2026. **No USB enumeration,
external display or Thunderbolt operation has been established on SiliconForest.**
The acceptance milestones below require the actual M1; source and build results
are recorded separately. Internal keyboard/trackpad are SPI, not USB evidence.

## Reference and baseline

Omacom `sources/gpu-plan/linux-aurora`, detached at
`6a08a08e5755`, is the port reference. HEAD was not moved. The existing remote
ref `origin/integration/aurora` is `39d5f88ab4e5`; its DWC3 source differs. Do
not silently combine the two versions. The local history at HEAD is shallow;
`git log --all -- drivers/usb/dwc3/dwc3-apple.c` also exposes newer fixes,
including `cda9c909149d` (set driver data before first cable connection),
`368d058a36b5` (fixed-hub handover) and `dc838cbcc3ff` (suspend-to-idle).
J293 has direct USB2 connections, not the newer J700 fixed-hub topology.

PureDarwin reference: `sources/puredarwin-rebase`; build uses the lab recipes,
not edits to that checkout. Existing patterns read: m1-apcie-host (bridge,
posted MMIO, DART), m1-bwfm (network interface and DHCP), m1-rust-kext,
m1-drm72, m1-lkpi-extras, m1-dcp and m1-gpu-dt/PDDispDT.

Before the first change, `m1-native-boot/install/build.sh
runs/m1-usbc/kit-base` reproduced GFX2's k2 `kernel.bin` **byte for byte**:
SHA-256 `7ad3b4a9e6e7e3e15ab038c9a61d824c6d1710f92b072ea418b207db911449d6`.
Comparison: `runs/gfx2/kit-k2/kernel.bin`; build log and kit remain private in
`runs/m1-usbc/`. This baseline includes `pdavd=1`, `if_attach_nx=46`, the
IO80211Family output counters and LinuxKPI taskqueue fix 0041. This is a local
rebuild comparison, not a new readback of SFBOOT. GFX2 confirmed k2 in the
coordination log at 00:54:36Z. No kernel install by this stream yet.

## Inventory and port order

Linux paths below are relative to the reference checkout.

| piece | Linux driver + path | what exists in SiliconForest today | port plan | size/risk |
|---|---|---|---|---|
| CD321x bus | `drivers/i2c/busses/i2c-pasemi-{core,platform}.c`; J293 `t8103-jxxx.dtsi` I2C0 | `PDI2CFamily` controller/client ABI; m1-sound `pd_i2c.cpp` already ports the polled PA Semi transfer engine, with reusable `pd_soc.cpp` power/MMIO | Reuse the existing Linux port for I2C0; serialize both CD321x clients on one bus. Start with port 1 state reads | small; shared bus also serves the protected serial port |
| PD, orientation, data role, alt modes | `drivers/usb/typec/tipd/{core,i2c}.c`, `tps6598x.h`, `cd321x-{dp,pm,setup}.h`; `apple,cd321x` | no CD321x driver | Port the CD321x branch, length-prefixed I2C blocks, APP/S0 setup, status/event handling and ordered mux/role callbacks; use its existing debounce and retry semantics | medium; incorrect role/reset order can kill serial or reset SoC |
| SPMI / ACE | `tipd/spmi.c`, same tipd core | no SPMI tipd transport needed for this board | J293 binds I2C addresses 0x38 and 0x3f. SPMI is a different transport, not a J293 prerequisite. The controller's firmware owns PD negotiation; host reads status and sends its 4CC commands. Do not invent an ACE RTKit endpoint or flash/update firmware | no J293 SPMI port; firmware interface needs live confirmation |
| ATC PHY / pipehandler | `drivers/phy/apple/atc.c`, `dptx.c`, `dptx.h`; `apple,t8103-atcphy` | no Type-C PHY provider; generic PHY declarations in DCP are insufficient | Port T8103 register/tunable, USB2, USB3, orientation, reset and PIPE routing paths first; keep DP/TB additions in same upstream ordering | large (whole atc.c 5,504 lines); calibration, power domains, cable-dependent reset |
| DWC3 Apple host wrapper | `drivers/usb/dwc3/dwc3-apple.c`, `core.c`, `host.c`, `dwc3-apple-state.h` | RavynXHCIPort has `snps,dwc3` support for an already powered host; no Apple lifecycle | Port Apple core setup, vendor registers and role/disconnect sequencing; publish an xHCI child only after PHY/core/PIPE ready | medium; leaving it running across disconnect loses subsequent devices |
| xHCI, hubs, USB device model | `drivers/usb/host/xhci*.c`, `drivers/usb/core/` | `RavynXHCIPort` is in the arm64 boot set, supports PCI, DWC3, hubs, polling/interrupts and hotplug; bridges `IOUSBFamily` through `RavynXHCIUSBBus` | Reuse host/class stack, add Apple prepared-host binding and translate **all** DMA allocations, contexts, rings and transfers through USB DART; audit teardown before detach | medium/large; current physical-address ring programming is not an Apple DART adapter |
| USB DART | `drivers/iommu/apple-dart.c`, `io-pgtable-dart.c`; two per-port DART instances, streams 0 and 1 | m1-apcie-host mapper is hardwired to dart-apcie0/stream1; DCP has its own mapper | Factor/use per-instance T8103 mapping for both USB stream paths, with invalidation and fault reporting; never reuse Wi-Fi stream/window | medium; wrong IOVA causes arbitrary DMA faults |
| HID | `drivers/hid/usbhid/` | `IOUSBHIDDriver`, keyboard/mouse, PDUSBHIDDevice, IOHIDFamily in boot set | Reuse; require physical keypress and event delivery after hotplug | small once host works; enumeration alone insufficient |
| storage | `drivers/usb/storage/`, UAS separately | IOUSB mass-storage sources and Ravyn xHCI BOT disk; IOStorageFamily, GPT/MBR, FAT/ext4 | Reuse BOT initially; explicitly mount external test media read-only by observed identity; no internal-disk-number assumptions | small/medium; UAS not established; never automount unknown media writable |
| direct USB Ethernet | `drivers/net/usb/{cdc_ncm,cdc_ether,r8152,ax88179_178a}.c` depending on actual adapter | `PDUSBNCM` source, IONetworkingFamily; NCM is not in upstream kc-arm64 boot list; no evidence of r8152/AX88179 or generic CDC-ECM support | Start with actual adapter class/VID:PID inventory; include NCM if applicable, port adapter driver otherwise; reuse pd-dhcp for new interface | hardware-dependent; do not claim all USB Ethernet is NCM |
| external DCP and scanout | `drivers/gpu/drm/apple/{dcp,apple_drv,connector,iomfb*,dcpext_scanout,dcpext_drm}.c` | PDDcp drives internal panel. External scanout functions in `kext/pd_dcp_linux.c` are stubs; pre-probe mailbox path is hardcoded `/arm-io/dcp`; PDDispDT supplies internal topology | Publish dcpext plus display/DART graph; per-instance RTKit/mailbox context; compile real external scanout and required helpers; preserve internal panel | large; second firmware instance, reserved memory, DMA and existing singleton assumptions |
| DP alt mode / link | `drivers/gpu/drm/apple/dptxep.c`, AFK/EPIC, connector/iomfb; `drivers/phy/apple/dptx.c`, atc.c; tipd HPD | dptxep is compiled, but its PHY/Type-C infrastructure and external topology are absent | CD321x DP status/HPD → Type-C mux → ATC lane/crossbar → dcpext dptx-port EPIC; firmware/link callbacks configure PHY; EDID/modes through display driver; KMS commit after link ready | large; both ports share one external display engine |
| ACIO / NHI | `drivers/thunderbolt/apple.c`, `nhi.c`, `ctl.c`, `tb.c`, `usb4.c` | no IOThunderboltFamily or Apple NHI port found | Port Apple ACIO power/tunables + M3 RTKit, child NHI DART, rings/interrupts and connection manager; retain Type-C out-of-band cable/orientation handoff | large; ACIO readiness is not dock authorization/enumeration |
| PCIe tunnel / dock USB and Ethernet | `drivers/pci/controller/pcie-apple.c`, Thunderbolt `tunnel.c`, `path.c`, `switch.c` | Wi-Fi bridge only drives apcie port 0; does not implement external apciec tunnel hosts | Add per-port apciec root/tunnel bridge, DART/RID/MSI, dynamic downstream scan/removal; then existing PCI xHCI and actual dock Ethernet driver | large; hot-remove DMA, bridge resources; dock chips must be observed |
| DP/USB3 tunnels | Thunderbolt `tb.c`, `tunnel.c`, `apple.c` DPIN handshake; ATC tunnel paths | absent | DP source is still dcpext, transported over TB to dock DP output. USB3 tunnel differs from PCIe-attached dock xHCI; enumerate topology to choose | large; DP is not a PCIe framebuffer merely because dock has PCIe devices |
| desktop outputs | Chonkstep `crates/wm-darwin/src/{kms,server,glrender}.rs`; Linux `wm-wayland` multi-output policy | Darwin Kms holds one connector/CRTC/plane, selects first connected; server creates one `PDGOP-0`. Linux frontend has multi-output/hotplug machinery, not automatically used by Darwin | DESK4 owns change: enumerate cards/connectors, maintain per-output commits/flip events, publish/withdraw wl_output, Samsung primary, fallback panel on removal | medium/large; do not modify compositor in this stream |
| hotplug userspace | Linux kernel USB/TB/DRM events; desktop connector policy, network service, storage policy | pd-netd is Wi-Fi-oriented; pd-dhcp takes an interface; no generic USB storage automounter established | Own wired-interface DHCP lifecycle and explicit read-only removable mount policy here; kernel must first enumerate real devices | medium; route/resolver ownership with Wi-Fi must be explicit |

## J293 wiring and sequencing

`arch/arm64/boot/dts/apple/t8103-j293.dts`: Type-C 0 is **USB-C Left-back**,
Type-C 1 **USB-C Left-front**. `t8103-jxxx.dtsi`: CD321x 0/1 are I2C0
0x38/0x3f, sharing GPIO interrupt 106. Connector graph ports 0/1/2/3 connect
respectively USB2 DWC3, ATC SS lanes, ACIO Thunderbolt and dcpext routing.
Both routes target **one** dcpext: at most one external monitor on this M1.

Port 0/1 DWC3 bases are 0x382280000/0x502280000, AIC 777/857;
USB DART pairs 0x382f00000+0x382f80000 and 0x502f00000+0x502f80000;
ATC PHY bases 0x383000000/0x503000000. These are Linux DT facts, not fresh
MMIO observations. Resolve actual ADT ranges before access.

The installed kit packs `usb-nodes=remove0`. `b1/src/pdsf_boot.c` prunes port 0
USB, PHY, ACIO, apciec/DART, HPM and DFP nodes; this protects the bench port.
Keep this setting. Physical bench cable location still needs verification;
do not infer it solely from node pruning. Start on preserved **port 1** only.

Upstream DWC3 order matters: configure PHY for cable/orientation; initialize
core without starting xHCI; finalize PHY and route PIPE; then start xHCI.
On detach stop xHCI before PIPE returns to dummy, then reset/stop PHY. CD321x
resets the eUSB2 repeater on cable changes, requiring a new PHY/core cycle.
A generic xHCI root-port poll alone cannot implement this lifecycle.

## Dock identity and Linux observation

The assignment names CalDigit TS3 (TB3), TS4 (TB4/USB4) and an unspecified
Kensington. No local doc identifies the Kensington model, and no live dock
topology was collected yet. Do **not** choose igb/i210 versus an Aquantia or
USB Ethernet driver from the enclosure name. Obtain `lspci -tv/-nnk`,
`lsusb -t`, `/sys/bus/thunderbolt/devices` product/authorization (excluding
unique IDs), DRM connector status/EDID metadata and relevant dmesg in one
Linux SSH session. Save raw output privately. No Linux configuration changes.
Inspect the current Linux desktop/network units in that same session to
establish its hotplug policy; no userspace service has been ported from it.

## Observable milestones

1. **Direct USB2/3:** implement I2C/CD321x, T8103 ATC, Apple DWC3 lifecycle,
   USB DART and xHCI binding; then ioreg enumeration, physical HID typing,
   read-only external storage mount, actual adapter DHCP/gateway. Repeat ten
   disconnect/reconnect cycles with UART captured, no panic or stale DMA.
   First implementation step: reuse PA Semi I2C port, publish CD321x state
   for the unreserved port. This step is not USB host support by itself.
2. **Direct DP alt mode:** real dcpext instance, PHY/crossbar, HPD, link,
   EDID/preferred mode, atomic scanout, compositor second output; replug.
3. **TS3/Samsung:** ACIO/NHI + authorization, PCIe tunnel to actual dock
   xHCI/Ethernet, USB device and DHCP; DP tunnel to Samsung. Dock replug.
4. **TS4/Kensington/adapters:** inventory actual models, apply tunnel/alt-mode
   paths appropriate to each; record hardware absent rather than passing it.

Each kernel install is one logical change on the verified base, using only
`install/linux/sf-bootbin-install.sh`, with rollback, two-minute coordination
announcement, gui.lock and M5 UART capture before its first boot. Default
boot remains Linux; return the shared machine to SiliconForest. Track current
state in `runs/m1-coord/status-CODEX-TB.md`. Sources/captures/identifiers stay
outside git. Ported GPL/MIT notices are retained per file and in kext NOTICE.
