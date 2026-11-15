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
coordination log at 00:54:36Z. The first installation and its evidence are recorded below.

## Inventory and port order

Linux paths below are relative to the reference checkout.

| piece | Linux driver + path | what exists in SiliconForest today | port plan | size/risk |
|---|---|---|---|---|
| CD321x bus | `drivers/i2c/busses/i2c-pasemi-{core,platform}.c`; J293 `t8103-jxxx.dtsi` I2C0 | `PDI2CFamily` controller/client ABI; m1-sound `pd_i2c.cpp` already ports the polled PA Semi transfer engine, with reusable `pd_soc.cpp` power/MMIO | Reuse the existing Linux port for I2C0; serialize both CD321x clients on one bus. Start with port 1 state reads | small; shared bus also serves the protected serial port |
| PD, orientation, data role, alt modes | `drivers/usb/typec/tipd/{core,i2c}.c`, `tps6598x.h`, `cd321x-{dp,pm,setup}.h`; `apple,cd321x` | `m1-typec` now reads port 1 over the existing polled I2C transport; APP/status/TBT observed in tc01. No setup/mux/role writes yet | Port the CD321x branch, length-prefixed I2C blocks, APP/S0 setup, status/event handling and ordered mux/role callbacks; use its existing debounce and retry semantics | medium; incorrect role/reset order can kill serial or reset SoC |
| SPMI / ACE | `tipd/spmi.c`, same tipd core | no SPMI tipd transport needed for this board | J293 binds I2C addresses 0x38 and 0x3f. SPMI is a different transport, not a J293 prerequisite. The controller's firmware owns PD negotiation; host reads status and sends its 4CC commands. Do not invent an ACE RTKit endpoint or flash/update firmware | no J293 SPMI port; firmware interface needs live confirmation |
| ATC PHY / pipehandler | `drivers/phy/apple/atc.c`, `dptx.c`, `dptx.h`; `apple,t8103-atcphy` | no Type-C PHY provider; generic PHY declarations in DCP are insufficient | Port T8103 register/tunable, USB2, USB3, orientation, reset and PIPE routing paths first; keep DP/TB additions in same upstream ordering | large (whole atc.c 5,504 lines); calibration, power domains, cable-dependent reset |
| DWC3 Apple host wrapper | `drivers/usb/dwc3/dwc3-apple.c`, `core.c`, `host.c`, `dwc3-apple-state.h` | RavynXHCIPort has `snps,dwc3` support for an already powered host; no Apple lifecycle | Port Apple core setup, vendor registers and role/disconnect sequencing; publish an xHCI child only after PHY/core/PIPE ready | medium; leaving it running across disconnect loses subsequent devices |
| xHCI, hubs, USB device model | `drivers/usb/host/xhci*.c`, `drivers/usb/core/` | `RavynXHCIPort` is in the arm64 boot set, supports PCI, DWC3, hubs, polling/interrupts and hotplug; bridges `IOUSBFamily` through `RavynXHCIUSBBus` | Reuse host/class stack, add Apple prepared-host binding and translate **all** DMA allocations, contexts, rings and transfers through USB DART; audit teardown before detach | medium/large; current physical-address ring programming is not an Apple DART adapter |
| USB DART | `drivers/iommu/apple-dart.c`, `io-pgtable-dart.c`; two per-port DART instances, streams 0 and 1 | m1-apcie-host mapper is hardwired to dart-apcie0/stream1; DCP has its own mapper; `m1-usb-dwc3` paired USB1 domain compiles as SDK objects only | Factor/use per-instance T8103 mapping for both USB stream paths, with invalidation and fault reporting; never reuse Wi-Fi stream/window | medium; wrong IOVA causes arbitrary DMA faults |
| HID | `drivers/hid/usbhid/` | `IOUSBHIDDriver`, keyboard/mouse, PDUSBHIDDevice, IOHIDFamily in boot set | Reuse; require physical keypress and event delivery after hotplug | small once host works; enumeration alone insufficient |
| storage | `drivers/usb/storage/`, UAS separately | IOUSB mass-storage sources and Ravyn xHCI BOT disk; IOStorageFamily, GPT/MBR, FAT/ext4 | Reuse BOT initially; explicitly mount external test media read-only by observed identity; no internal-disk-number assumptions | small/medium; UAS not established; never automount unknown media writable |
| direct USB Ethernet | `drivers/net/usb/{cdc_ncm,cdc_ether,r8152,ax88179_178a}.c` depending on actual adapter | `PDUSBNCM` source, IONetworkingFamily; NCM is not in upstream kc-arm64 boot list; no evidence of r8152/AX88179 or generic CDC-ECM support | Start with actual adapter class/VID:PID inventory; include NCM if applicable, port adapter driver otherwise; reuse pd-dhcp for new interface | hardware-dependent; do not claim all USB Ethernet is NCM |
| external DCP and scanout | `drivers/gpu/drm/apple/{dcp,apple_drv,connector,iomfb*,dcpext_scanout,dcpext_drm}.c` | PDDcp drives internal panel. External scanout functions in `kext/pd_dcp_linux.c` are stubs; pre-probe mailbox path is hardcoded `/arm-io/dcp`; PDDispDT supplies internal topology | Publish dcpext plus display/DART graph; per-instance RTKit/mailbox context; compile real external scanout and required helpers; preserve internal panel | large; second firmware instance, reserved memory, DMA and existing singleton assumptions |
| DP alt mode / link | `drivers/gpu/drm/apple/dptxep.c`, AFK/EPIC, connector/iomfb; `drivers/phy/apple/dptx.c`, atc.c; tipd HPD | dptxep is compiled, but its PHY/Type-C infrastructure and external topology are absent | CD321x DP status/HPD → Type-C mux → ATC lane/crossbar → dcpext dptx-port EPIC; firmware/link callbacks configure PHY; EDID/modes through display driver; KMS commit after link ready | large; both ports share one external display engine |
| ACIO / NHI | `drivers/thunderbolt/apple.c`, `nhi.c`, `ctl.c`, `tb.c`, `usb4.c` | no IOThunderboltFamily or Apple NHI port found | Port Apple ACIO power/tunables + M3 RTKit, child NHI DART, rings/interrupts and connection manager; retain Type-C out-of-band cable/orientation handoff | large; ACIO readiness is not dock authorization/enumeration |
| PCIe tunnel / dock USB and Ethernet | `drivers/pci/controller/pcie-apple.c`, Thunderbolt `tunnel.c`, `path.c`, `switch.c` | Wi-Fi bridge only drives apcie port 0; no external apciec tunnel host. Existing `PDFBSDem` source/personality includes I210 `15338086`, but is omitted from the M1 boot set | Add per-port apciec root/tunnel bridge, DART/RID/MSI, dynamic downstream scan/removal; then existing PCI xHCI and existing PDFBSDem/I210 after auditing its DMA path | large; hot-remove DMA, bridge resources; dock chips must be observed |
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

The first Linux survey was collected during tc01, without configuration changes
(`runs/m1-usbc/tc01-linux.txt`, private). The owner-connected dock identifies as
**CalDigit TS3 Plus**, Thunderbolt generation 3, `authorized=1`, on domain/port
1. Linux enumerates its PCIe tunnel and these endpoints:

| endpoint | PCI ID | Linux binding | SiliconForest reuse |
|---|---|---|---|
| two Fresco FL1100 USB controllers | `1b73:1100` | xhci_hcd | RavynXHCIPort, once PCIe tunnel/DMA exists |
| ASMedia ASM1142 USB controller | `1b21:1242` | xhci_hcd | same |
| Intel JHL6540 USB controller | `8086:15d4` | xhci_hcd | same |
| Intel I210 Gigabit Ethernet | `8086:1533` | igb | existing PDFBSDem source/match; not yet loaded/tested on M1 |

Thus this dock's Ethernet is **PCIe I210**, not an assumed USB Ethernet or
Aquantia chip. USB audio/hub/HID interfaces also enumerate behind the dock in
Linux. This does not establish any of those endpoints under Darwin.

Linux reports external `card3-DP-2` connected, with 3840x2160 among its modes,
and `card3-eDP-1` connected at 2560x1600; DP-1 is disconnected. The assignment
identifies the Samsung as the dock monitor; no EDID metadata was collected to
independently establish its model. NetworkManager and bolt.service were active,
systemd-networkd inactive. The Type-C port1 PD role was device/sink while its
PCIe tunnel was active: this is not a reason to start DWC3 as a peripheral.
CD321x's direct USB controller role uses DATA_STATUS and is NONE for this
TB-only connection (no USB2/USB3 connection flags).

No TS4 or identified Kensington was present in this capture. The Kensington
model remains unknown; no local doc names it. Future surveys should also save
EDID metadata and the network service's interface/route policy privately;
current evidence establishes service presence, not its complete hotplug policy.

## First implementation and real M1 evidence

- `d41d97274`: initial source survey; `32270c69d`: `m1-typec`, reusing
  `m1-sound/kernel/pd_i2c.cpp` (the Linux PA Semi port) and `pd_soc.cpp`.
- tc01 (01:07:57–01:10:42Z, gui.lock and bench queue): clean Darwin reboot,
  one Linux survey/install session, `sf-bootbin-install.sh`, one-time bless,
  M5 UART capture opened before the new boot with routing retried through reset.
  Linux `--show` verified the installed capsule hash:
  `f10ca750111b5dca507b2435799571d2c2d15186afa294ff8244455ac012a861`;
  `.old` is k2 `7ad3b4a9…`. `boot.bin` remains `81d58f1b…`.
- **Observed in Darwin:** `PDAppleTypeC` registered/matched; I2C0 revision 6,
  divider 15; HPM1 vendor register nonzero, firmware mode `APP `;
  `status=0x100cb20d`, `data-status=0x16110081`, cable present, normal
  orientation, Thunderbolt flag set, USB2/USB3/DP/USB4 flags clear. At the
  post-boot check: 62 samples, zero errors; later 612 samples, still zero errors. The sampler does not send PD
  setup/4CC/IRQ-clear commands and does not start a PHY or host controller.
- Capture `tc01-uart.txt`: new Darwin banner, no panic line through the
  captured first-boot interval. The compositor `.next` and inject directory
  were restored before releasing the lock. **Capture correction:** the owned
  reader initially remained open after release; stopped at 01:15:24Z. GFX2
  was notified that any intervening captures may have shared UART bytes;
  supplemental bytes are in `tc01-uart-final.txt`. The first tc01 boot
  predates that overlap. Future cycles must stop/reap their reader before
  releasing gui.lock.
- `7812ea1f5`: subsequent source-only change distinguishes the PD role from
  DATA_STATUS's USB role and publishes one immutable `port-state` OSData
  snapshot for kernel clients. Host framing/role tests and SDK build pass.
  **Not installed**: the disk still runs the `32270c69d` sampler.
- `9d07d0958`: `m1-usb-dwc3` paired USB1 DART domain/owned DMA buffers,
  ARM64 SDK objects built; reused table tests pass. **No DART hardware test,
  xHCI integration, DMA, USB enumeration or hotplug qualification yet.**
  The current xHCI stop/free path also needs worker joining before a PHY/core
  reset can safely remove and replace an entire controller.

What a person can do now: leave the TS3 attached and inspect its Type-C cable
status in Darwin using `/private/var/db/sftb/ioreg -rc PDAppleTypeC -l -w0`.
That private copy is the open-source PureDarwin ioreg build, staged/hash-checked
because the installed system did not have ioreg. They cannot yet use dock USB,
Ethernet or external display under Darwin. No physical plug/replug or HID
input was generated by this stream.

## Recovery finding and validation

The Linux SFBOOT listing still contains the original **14,423,700-byte**
`boot.bin.old` (`8f300c87…`), which stage 1 refuses. The old `--rollback` branch
blindly swapped both old files and would reinstate it. `553aae9d0` validates
**the complete resulting pair on a read-only mount before any rename**; it
refuses that old pair and preserves the current files. Normal installation
of the verified base kit's `boot.bin` plus `kernel.bin` remains the recovery
route to k2: use the installer with both files from
`runs/m1-usbc/kit-base`, copied to Linux. Do not use an older copy of the
installer's unvalidated `--rollback`.

Seven scratch-file tests execute the actual rollback branch (mount commands
recorded): new script 7/7, original script fails four refusal cases. Kit tests
11/11 pass against tc01. No real hardware rollback was needed or attempted.
Linux `e2fsck -fn` before tc01 returned 4 with existing crash/restart findings;
this stream did not repair the root. All six AGENTS.md repository checks passed again after the rollback/source changes;
focused SDK/host checks are recorded separately.

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


## Next implementation boundary

The installed tc01 image is an experimental collection built by
`m1-typec/build-kc.nix`, not a change to the default rebase collection recipe.
A default kit rebuild currently **omits the Type-C kext**. To reproduce the
installed image, retain the immutable tc01 KC store path recorded above and
pack it with `SF_KC=<that path> SF_EXTRA_BOOTARGS=pdtypec=1`; the private kit
is already preserved. Building the current Type-C recipe instead includes
7812ea1f5's later, SDK-only snapshot change and is a new candidate.

Next is the actual host path: wire xHCI ring/context/transfer allocations to
the paired DART adapter (initial MSC traffic must bounce too), implement
T8103 USB2 ATC/DWC3 setup on the preserved port, and serialize/drain xHCI
workers before resetting that controller on cable removal. Then USB3 tuning
and physical acceptance. PD versus direct USB role must stay distinct: this
TS3 cable reports TB without a direct USB connection, so it must not start
DWC3. Direct keyboard/storage/Ethernet hardware and a USB-C display adapter
have not been observed attached; the status file requests them without
waiting for answers. Milestone 1 remains unfinished.
