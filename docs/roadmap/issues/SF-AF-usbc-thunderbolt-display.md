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

### xHCI DMA and removal source, 2026-11-15 02:30Z

`m1-usb-dwc3/build-xhci.nix` now SDK-builds a replacement RavynXHCIPort
with the paired USB1 DART backend. All thirteen xHCI allocation sites and all
buffer address sites use the adapter on Apple, including MSC bounce payloads.
The IOVA allocator also honors physical-mask alignment: a 64KiB-aligned CPU
buffer alone does not keep a translated single-TRB transfer within 64KiB.
PCI/non-Apple allocation paths remain the upstream path.

Removal closes new call admission and drains retained hotplug/bulk workers
and active class operations before requiring hardware halt/reset. Failed
drain/reset retains DMA; `PDAppleUSBDrain` returns busy and the parent may not
reset the PHY. Old USB bus/storage clients retain the controller until they
finish, and successful stop aborts transfers and terminates children. The
provider's `pd-usb-dma-live` property covers partial attachment too.

Host ASan/UBSan checks passed 100 concurrent removal cycles and 4,194,304
fragmentation/alignment cases. The ARM64 SDK bundle builds; six repository
checks pass. This source is **not installed** and no xHCI DMA, enumeration,
HID, storage mount, DHCP or ten-cycle USB hotplug has been observed. ATC/DWC3
parent lifecycle integration is the next step; this is not milestone 1 done.

### ATC USB2 and DWC3 lifecycle source, 2026-11-15 02:35Z

The experimental `m1-usb-dwc3/build-kc.nix` now builds the complete host
bundle plus the Type-C snapshot provider, replacing the original xHCI
bundle once. The collection passed its segment overlap check (zero pairs).
`pdtypec=1 pdusbhost=1` enables the port-1 USB2 lifecycle; the daily kernel
recipe is unchanged. This collection is not installed at this checkpoint.

The `m1-atcphy` port has external reset/clamp, dummy PIPE parking with
acknowledged live handoff, USB2/common power and status polling, T8103
post-tunable setup, lanes-OFF/orientation routing and shutdown. Its kernel
adapter validates the complete ADT common tunable group and actual register
layout before PHY writes, reads port-1 fuse fields, applies the original
common-a/AXI2AF/common-b order, prepares paired DART while DWC3 is reset,
initializes DWC_usb31 and publishes the prepared xHCI nub. USB2 HOST is
selected after core reset, as required by the current Linux Apple glue.
DWC3 identity/capability and reset timeouts are checked; gadget event DMA
is disabled. Removal refuses PHY reset until xHCI drain and synchronous
termination finish, including the failed-attachment DMA case.

Host ASan/UBSan checks cover 100 USB2 power/orientation cycles, failed PIPE
acknowledgement, power/tunable failures, 16,384 compact-record cases and
DWC3 core/reset/role ordering. These are register models, not hardware
observations. SuperSpeed, DP/Thunderbolt PHY modes and actual USB device
behavior remain unfinished.

At 02:32Z a read-only M1 sample still reported the dock's Thunderbolt-only
cable state, no USB2/USB3/DP flags, 3,217 samples and zero errors. The source
host lifecycle deliberately defers that state; dock USB requires NHI/PCIe
tunneling. Direct USB devices remain requested from the owner. Work proceeds
on the Thunderbolt and external-display source/host paths without waiting.

### USB02 first boot and packaging failure, 2026-11-15 02:44Z

USB02 capsule `b43f3cb93fe86b6e14d2da2f0a3296dc93b8c1bc8c1f635849f3b9c30c1d9b37`
from KC `/nix/store/9k2xml5v9xzyn9jw94p2p0hplzzl80ki-puredarwin-kc-arm64-0.1`
booted under gui.lock/bench after Linux installation. UART captured the Darwin
banner and no panic marker. The updated Type-C snapshot/USB-role source runs
on M1 and reports the same TS3-only state, USB role none, 62 samples/0 errors.
The xHCI/PDAppleUSB bundle **failed to load**: its new versioned dependency
on PDAppleTypeC could not be satisfied because that observer is not declared
a versioned library. No ATC/DWC3/DART operation ran from the host service.
The reader was explicitly reaped before releasing gui.lock (02:43:58Z).

`638e824ef` removes that unnecessary dependency: the host uses IOService
properties and links no Type-C class symbols. USB03 collection/kit and 11 kit
checks pass; the hardware retry is pending DESK4's announced session/reboot
window and its explicit request to wait for its done line. `b3d366f26` also
fixes the synthetic nub's name/resource overrides before any host attachment:
its parent is PDAppleUSB, so IOPlatformDevice must not cast that parent to
IOPlatformExpert. The rollback baseline remains the verified tc01 kit;
the default boot remains Linux. Nothing was pushed.

### External-display / Thunderbolt source backends, 2026-11-15 02:59Z

`m1-atcphy` now carries directly extracted T8103 DP AUX, four DP link-rate
PLL/lane programs and the distinct Thunderbolt DP pixel-clock program,
with original SPDX/copyright and a reproducible extraction tool. Native
callbacks replace register access/delay; the caller must hold the PHY lock
and already own a powered/calibrated mode. Failed tunnel-clock APB shutdown
retains ownership and refuses retuning. Host ASan/UBSan covers rates/PMA
masks, repeated-rate behavior, shared-PLL refusal and command/lock/stop
failures. The DP programs SDK-compile; none has trained a link on M1.

`m1-thunderbolt` retains upstream DP-IN state/token/provider code and its 35
tests byte for byte (128,371 assertions through a host fixture adapter,
not Linux KUnit/hardware execution). Its SDK objects additionally port Apple
NHI ring budgets/layout/PDF, atomic VALID/E2E setup, descriptor encoding,
RTKit SRAM bounds, the M1's single-budget HPD/INACTIVE_ACK handshake and
ACIO startup/unwind order. Native platform/RTKit/DART/control-channel/CM and
PCIe-C bindings remain missing; there is no attached Thunderbolt driver.
Register-model cases cover failed disables and every startup/teardown step.

`m1-dptx` validates external dcpext/dispext register resources, mailbox IRQs,
carve-outs 73/74, firmware compatibility and the port-1 routing plan; its
223 missing/truncated-property cases and overflow/ABI refusals pass. It has
no MMIO or synthetic-tree publication yet. T8103 uses the already compiled
normal IOMFB pipeline, rather than the later-chip `dcpext_scanout` path.
Two integration gaps were confirmed in the existing DCP adapter: it binds
only the first display `iommus` entry, and its DMA free paths disregard TLB
invalidate failure. A second output requires shared framebuffer IOVAs in
both display DARTs and retained backing on failed unmap/flush, along with
real PHY/crossbar/Type-C providers and the external RTKit mailbox binding.
No HPD/training/EDID/mode/second-output or TS3 tunnel success is claimed.

### USB03 captured boot: lifecycle service attached, 2026-11-15 03:06Z

USB03 capsule `7ae7d4240745c5450886ce40adbdb276572a3313a6fab253e7ae86131c0913f6`
from KC `/nix/store/sh3yw7qb1zh3ixwnx8xkpvxppka3iqrb-puredarwin-kc-arm64-0.1`
booted after DESK4's release and PERF's guarded libobjc install/checks. The
cycle held gui.lock and bench, installed through the validated Linux script,
used one-time boot, captured UART before first boot and stopped/reaped its
owned reader before lock release at 03:06:08Z. Darwin banner was captured;
no panic marker was found. The .next/inject desktop returned.

Both PDAppleTypeC and PDAppleUSB are now registered/matched on real port 1.
The unchanged TS3-only cable state reports USB role none, host-active No,
alternate-mode-deferred Yes, removal-blocked No and zero sampler errors
(61 samples at capture). The packaging dependency failure from USB02 is
resolved. This establishes driver loading and deferred-mode behavior only:
there was no direct USB device, no ATC/DWC3 host initialization or controller
DMA, and no enumeration/HID/storage/DHCP/hotplug proof. The first direct
plug remains a captured risky test to coordinate through the status file.
The newer six-mode/calibration source work after this boot is not installed.

### Six-mode ATC source and runtime calibration, 2026-11-15 03:16Z

The ATC backend now ports USB2, USB3, USB3+DP, TBT3, USB4 and DP mode
programs in both orientations. Runtime ADT calibration grouping follows
m1n1: USB groups on the selected PMA plus DP TX on the other, both DP TX
groups for DP, and both sets of CIO groups for TBT/USB4. Entire bundles
are preflighted before PHY power writes and revalidated before applying;
calibration failure blocks lane routing and reset release. No calibration
payload is committed. TBT3 keeps PIPE dummy, distinct from USB4's backend.

ASan/UBSan passes the twelve mode/orientation programs, ten calibration
bundles, 56 missing properties, changed-program/late-malformed refusals,
and existing power/DP cases. SDK object output is
`/nix/store/71bbh5x17137bx12ks9k4vk0wcbsmb01-puredarwin-atc-objects-0.1`;
the existing USB2 native bundle still builds with the shared power program
(`/nix/store/4ksp2ljj2fv153asgk0xa7zdyjl52cy6-puredarwin-apple-usb-host-0.1`).
All six repository checks pass (274 Python tests, 77 C groups). USB03
remains installed; native lane-calibration selection and safe USB3 PIPE
routing are the next attachment work. This is source/SDK evidence only.

### Direct USB3 native attachment prepared, 2026-11-15 03:22Z

The USB adapter now selects USB3 from the Type-C snapshot, copies and
retains its runtime calibration before PMGR/PHY writes, programs the
orientation, initializes DWC3's host role and routes PIPE before publishing
xHCI. USB2/USB3 changes drain the previous session. T8103 skips host BIST
as the qualified Linux workaround does. Shutdown uses stopped parking
only after the existing controller drain and synchronous child termination.
Failed handoff completion retains PHY ownership. The child iterator is
released after retaining its client, avoiding a reference leak each removal.

Host tests pass 100 PIPE cycles, live lock retries and field restoration,
clockless stopped parking and failed-completion retry, plus all existing
USB DMA/lifetime and ATC tests. The native bundle and USB04 collection build;
USB04 KC is `/nix/store/1z98228ka748b3ak671wadlwbrskhsnh-puredarwin-kc-arm64-0.1`,
capsule SHA256 `03d39b8176db0069c397e2d9f979a087f853b25626c4aadec408148c26f7febf`.
Its unchanged boot.bin hash is `81d58f1b40e8139dc797ceac8314bb74a6308ccf18b28e42f1c902c7b4912e3e`;
all 11 kit checks and six repository checks pass. Installation is announced
for after PERF's measurement (no earlier 03:31Z), subject to a fresh log
check inside gui.lock. USB03 remains installed at this checkpoint; neither
controller DMA nor direct-device enumeration has been observed.

### ACIO tunables, CM core and native RTKit source, 2026-11-15 03:28Z

ACIO's runtime decoder now validates the actual 24-byte records and all
eleven RC/NHI/PCIe-adapter groups before any write. It preserves Linux's
independent clear/OR-set semantics and skips unchanged registers. ASan/UBSan
passes 110 late missing/malformed refusals and all eleven base mappings;
SDK objects are `/nix/store/qkqxcpbyg4npxmfqn7caial2hvihzjbp-puredarwin-thunderbolt-objects-0.1`.
No firmware/calibration/device-identity bytes are committed.

The actual pinned Linux CM core (ten control/path/tunnel translation units)
SDK-compiles against LinuxKPI, with endian-array/CRC32C wire helpers and
tracepoint instrumentation disabled in the staged copy. Output:
`/nix/store/mk0abkh7l44yl8alnnn2pwx3hy7akzy7-puredarwin-thunderbolt-cm-objects-0.1`.
This is not linked or attached. Full-CM compile attempts remain failures:
LinuxKPI router bus/device/PM/uevent/property/crypto/cleanup APIs still need
native implementation. Logs remain private; no missing backend is replaced
with a success stub. The known wire vector and 514 array cases pass.

Native RTKit platform initialization now serializes allocation/publication
and retains its host anchor; failed allocation cleans up and permits retry.
The resolver supports ACIO1's M3v2 mailbox directly at reg0 (no ASC 0x8000
offset), while preserving ASCv4 for internal/external DCP. Port0 M3 remains
refused. Layout tests pass 48 IRQ permutations, every undersized window,
overflow and malformed IRQs. The complete PDDcp bundle SDK-builds as
`/nix/store/6wjx0zwiv0h2nbcf2cp05yw0nz8h5vfv-puredarwin-dcp-0.1`.
These newer RTKit/DCP changes are not part of USB04 and establish no M3
boot, external DCP, trained link or scanout. DESK4's K1-K12 interface request
has been read and answered; actual connector evidence must precede its
second scanout integration request.

### USB04 captured boot, 2026-11-15 03:34Z

The frozen USB04 capsule `03d39b8176db0069c397e2d9f979a087f853b25626c4aadec408148c26f7febf`
was installed after PERF finished and TERM6 released its hold; the log was
re-read inside gui.lock before the cycle. Linux normal installer and one-time
boot were used, with UART active before the new kernel's first boot. It
returned to SiliconForest and .next/inject by 03:34:02Z. The owned reader
was stopped/reaped at 03:34:03Z before lock release. UART records the Darwin
banner and no panic(cpu) report. The pre-existing read-only fsck findings
match the documented 21:06Z remnants (inode 65144, blocks 636291–636295,
group 17 and total free-count delta 3); no filesystem repair was performed.

PDAppleUSB is registered/matched and reports `super-speed-capable=Yes`.
The unchanged TS3-only cable remains deferred, host-active No, removal-blocked
No; Type-C sampler 61 samples and zero errors at capture. This proves new
bundle loading/deferred behavior only. No ATC USB host initialization,
controller DMA, enumeration/HID/storage/DHCP/ten cycles, NHI/tunnels or
external output has been observed. The separate RTKit/hotplug source after
this collection build is not installed by USB04.

### Darwin hotplug observer source, 2026-11-15 03:37Z

The DCP hotplug worker now queues a dedicated native cache refresh after
real upstream notifications, including route removal. It refreshes modes,
then publishes cached connector status/name/preferred mode and a monotonic
read-only `kern.pd_dcp.hotplug_seq`; `kern.pd_dcp.connectors` serves the
snapshot. Sysctl reads hold only a short metadata mutex. No event, EDID or
connector is fabricated. DRM ioctl latency remains subject to DRM locks
and must be measured on hardware; no 50 ms guarantee is claimed. The
complete DCP bundle SDK-builds; this code is not in installed USB04. Native
hotplug/connector/scanout evidence remains outstanding.

### DCP DMA lifetime prerequisite, 2026-11-15 03:46Z

The DART backend now provides checked retirement with preflight table
walks and real invalidate error propagation. An unreachable table or
failed invalidate poisons its stream; any poisoned DCP/PIODMA/display
stream closes allocation/map/IOVA-reclaim across the backend because they
can share backing. Caller producer/in-flight quiescence remains separate.
Owned allocations retain pages/views/IOVA/device refs on failed cleanup.
PRIME retains exporter DMA-buffer refs and pins on failed import cleanup
or destruction, including failures before the import GEM exists. The
physical-page unmap API now returns failure instead of silently freeing.

The native template tracks PIODMA ownership on T8103's older firmware as
well as later firmware. Failed first-segment rollback is flushed too.
Declared-but-unavailable IOMMUs fail closed instead of falling back to
physical DMA, and live bindings cannot change streams. Generic SG use in
this driver is backed by owned DCP allocations; future arbitrary borrowed
callers still need their own exporter lifetime contract.

ASan/UBSan passes existing table checks and 512 retirement cases including
timeout/poison retry, unreachable tables and invalid ranges. Full PDDcp SDK
output: `/nix/store/fakwnsdwpmqgh4iqfmgs1bcyyav9abma-puredarwin-dcp-0.1`.
This newer source is not installed in USB04. Mirrored framebuffer IOVAs in
both display DARTs, external topology, native PHY/mux/Type-C and actual
external DCP/link/scanout are still required.

### Mirrored framebuffer DMA source, 2026-11-15 03:54Z

The display DMA adapter now consumes one or two complete T8103 IOMMU
specs, reserves the same IOVA in both native bitmaps under one lock, and
maps/flushes/unmaps the full domain for coherent, physical-page, SG and
resource paths. It publishes only after both invalidates succeed. A peer
map failure preserves a pre-existing firmware PTE; outer allocation
cleanup clears only successfully mapped pages. Either stream's retirement
failure preserves all backing and both address reservations. PRIME's
translation check compares both banks. Capacity covers four DART blocks
and eight streams; no TTBR or firmware IOVA is invented.

ASan/UBSan passes 524,288 paired and 2,048 single-bitmap cases, actual table
pairs, foreign-PTE-preserving rollback and both-stream map/flush/retire
failure cases, plus the earlier table/512 retirement cases. Full SDK output:
`/nix/store/7dcmfyxzsdzvgmam6w5ai5x1zmms35z6-puredarwin-dcp-0.1`.
All declared DART streams must be valid/powered and retain their tables
while shared buffers exist. These are source/SDK checks, not live external
DMA. The current tree still publishes only the internal display path;
external topology, real PHY/mux/Type-C providers and external DCP/link/
scanout remain next work. USB04 remains installed.

The initial mirrored-binding parser was too narrow (SID-only); source
review caught the existing display builder's five argument cells before
installation. It now accepts the actual one/five-cell forms and consumer
aperture override, checks every property cell to reject malformed tails,
and refuses unsupported windows rather than allocating beyond them. A
host integration test passes all three bindings emitted by the actual
internal display builder. The corrected SDK attempt also exposed a missing
LinuxKPI u64-array reader; the final adapter uses its real u32-array reader
and combines the pairs. The failed build receipt remains a failure.

### T8103 DP/DP-IN crossbar source, 2026-11-15 04:09Z

`m1-dptx` now ports the upstream T8103 crossbar register bodies for the
physical DP PHY and both Thunderbolt DP-IN outputs. The reproducible
extractor retains source provenance/notices and T8103's two-input and
cycle-slip policy. The program includes duplicate-input refusal before
writes, DP-IN preselection, link clock stop/start and disconnect with
unrelated fields preserved. The caller still owns native power, mux lease,
shared ATC/crossbar serialization and prior-user drain; init proves no
hardware idle fact. Gate status remains diagnostic, matching upstream.

ASan/UBSan passes 192 PHY/DP-IN select/link/disconnect cycles and conflict,
preselection, shared-field, gate-order and stuck-status cases. SDK objects:
`/nix/store/iz2ln1z80d4krl854krjrlfzshavyafn-puredarwin-dcpext-plan-objects-0.1`.
Six repository checks pass. No crossbar writes, HPD, link training, NHI
traffic or scanout have run from this source on M1. USB04 stays installed;
external topology/provider and router-device integration continue.

### Router device/reference adapter, 2026-11-15 04:33Z

`m1-thunderbolt` now SDK-compiles nineteen upstream manager files plus two
native registry/device adapter objects. The recipe asserts the full/core
object count (21/12), presence/absence of `domain.c.o`, and emits defined,
undefined and remaining external symbol manifests. An evidence correction:
initial full attempts through `tb-cm-full-08` only compiled the ten core
files because the inherited configure phase ignored `cmakeFlags`. These
receipts are not full-manager proof. Passing the option in the actual
configure command exposed additional errors; the failed receipts remain.
The corrected full native SDK build is
`/nix/store/yma5kcgm68n6vpb0l03xk60lacynqzr5-puredarwin-thunderbolt-cm-objects-0.1`;
core-only is `/nix/store/fspxvd6p2dmrhrg1ah2jczjfzr27m2yf-puredarwin-thunderbolt-cm-objects-0.1`.

The native adapter supplies real metadata operations and reference-safe
bus/child lookup, publication rollback, type release and bounded events.
An iterator pins both child and parent; each child's separate parent
reference lasts until final release. Structural changes require native
owner serialization. Runtime-PM calls bypass the SDK's success/no-op
helpers: actual native callbacks are mandatory before domain init, errors
block new power/publication operations, and teardown traversal remains
possible. OS information also requires a real native snapshot. Optional
XDomain service-driver registration fails unsupported rather than reporting
an unimplemented binding. Private staged LinuxKPI layout additions retain
notices. The x86 Intel Apple NHI resume-link function is excluded on ARM;
protocol/control bodies and runtime callback pointers remain intact.

ASan/UBSan passes the production registry and device adapter using host
reference/lock/publication fixtures: 100 self-removing trees with final
parent lifetime, eight concurrent iterators, 1000 remove/re-add cycles,
capacity/overflow/reference failures, watermark/reverse lookups, rollback,
event bounds and native PM failure followed by cleanup. Logic/registry SDK:
`/nix/store/wrjjx0s261vvxj2pc6kkz315qrxf7vvi-puredarwin-thunderbolt-objects-0.1`.
All six repository checks pass (274 Python, 77 C groups, six mutations).

This is not a linked/attached CM or full dependency closure. Native IORegistry/
PM callbacks, USB4 port-device/quirks sources, crypto/UUID bindings, Apple
NHI control DMA/interrupts, ACIO M3/RTKit/DART and PCIe-C remain. No ACIO
writes or tunnels have run. USB04 is still the installed capsule and TS3
remains deferred; no hardware milestone is marked complete. Continuing
native attachment and external provider work without another reboot yet.
