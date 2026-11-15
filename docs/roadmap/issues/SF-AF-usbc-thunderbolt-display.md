# SF-AF: USB-C, Thunderbolt and external display on J293

Owner: CODEX-TB. Survey started 15 November 2026. **No dock PCI/USB enumeration or external display has been established
on SiliconForest; ACIO M3, NHI control DMA/IRQs and host-router startup
are now observed ready.**
The acceptance milestones below require the actual M1; source and build results
are recorded separately. Internal keyboard/trackpad are SPI, not USB evidence.

## Current target and observed state (owner change; updated 12:23Z)

The CalDigit TS3 Plus remains attached to left-front Type-C port 1 and is
now the **first hardware target**. Direct USB/DWC3 and a USB-C DP adapter are
retained source paths, deferred until the owner makes that hardware available;
no further direct-USB-only boots are planned. The TS3 is Thunderbolt 3-only:
its USB ports cannot exercise the M1's direct DWC3 host before a PCIe tunnel
and PCI xHCI are operating.

**Observed in Darwin:** non-AVE CIO17, source `83786dc6e`, capsule
`fcf8f72d697e…`,
boots the internal desktop/Wi-Fi and reports RTKit/NHI ready, connection-manager
startup complete with errno0, PHY owned and no quarantine. TX0/RX0 options
read `0xc0000000`; IRQ enable is `0x1001`, matching the live Linux control
ring state. The actual host-router service `0-0` and `usb4_port1` publish
beneath the native domain. Full-bank DART reset removed the early SID15 fault;
Linux and Darwin now have SID1 translating and unused streams disabled.
The one-shot link query reports both physical lanes UNPLUGGED (state7),
no remote router, count1/error0. The owned UART reader was reaped before
lock release at12:20:29Z. An explicit late snapshot also reports state7/remote0;
UID65534 was denied without incrementing the query count, while root completed
one request and advanced it1->2; the post-idle query advanced it to3. CD321x power7 was changed to S0 with the bounded
SSPS task (two checks, readback/error0). AVE is paused; this collection has no AVE module
or `pdave` argument. CIO17 also acknowledges one real CD3217 EVENT1 word,
with zero errors and EVENT1 cleared; the mask stays unchanged. Both lanes
remain7/remote0, so acknowledgement alone did not establish the link.

**CIO16 failed qualification:** the same non-AVE diagnostic image reached
healthy NHI/RTKit/Type-C/PHY startup, but the standard desktop and inject
directory were absent at75s. It was restored to qualified CIO15 at11:45:55Z;
the owned UART reader was reaped at11:45:56Z before lock release. No new
compositor-start record or first SF_SESSION marker was captured. The cause
is unresolved. A same-image CIO16B repeat is announced for12:00Z or later,
with process/launchd state capture and bounded additional session waiting.
That repeat qualified at12:03:54Z (UART reaped12:03:55Z): the normal desktop
was already present at75s and all snapshot/privilege/ABI checks passed. The
earlier startup failure has not reproduced and its cause remains unknown.

**Not observed in Darwin:** the dock router, authorization, any dock PCI
function, Ethernet link/DHCP, dock USB enumeration/HID/read-only storage,
HPD/EDID/external scanout or physical dock hotplug cycles. Host-router
readiness is not dock discovery. PCIe-C cold-init/link helpers, endpoint
DMA/bridge work and external-display sources remain separate source/host
evidence. DESK5's second-output userland half is built; K1–K13 remain the
kernel contract. CIO01/CIO02/CIO04–06 failures and recoveries are retained
below, not relabeled as successful boots.

The shortest path is shared port-1 PHY ownership → TBT ATC calibration →
ACIO M3/RTKit power/reset/firmware and SID1 DART → NHI DMA/interrupts → the
software manager's host/router enumeration and authorization → apciec1
PCIe tunnel/root/hotplug → existing I210 and PCI xHCI drivers → dock DP-IN
and dcpext. Each boundary requires a Darwin register/trace observation
compared with the saved Linux TS3 trace before calling the next stage ready.
Bench Type-C port 0 stays reserved. Ordinary Wi-Fi PCIe port 0 is a different
namespace from that protected USB-C port; retain its working bridge.

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

Linux paths below are relative to the reference checkout. This table retains
the initial survey inventory; the current target above and acceptance order
below supersede historical next-step notes.

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

The owner revised the acceptance order on 15 November 2026:

1. **Dock PCI enumeration first.** Keep TS3 attached. Observe current TBT
   negotiation, acquire the shared ATC lease without taking the bench port,
   validate/power the ACIO1 M3/RTKit/DART/NHI chain and its interrupts, then
   enumerate/authorize the actual router. Extend the existing native bridge
   to the Type-C port-1 `apciec1` tunnel root, with separate ECAM, RID/DART,
   MSI and downstream removal ownership. Acceptance is Darwin ioreg/PCI
   inventory of the saved Linux endpoints: two `1b73:1100`, `1b21:1242`,
   `8086:15d4`, and I210 `8086:1533`. No PCIe devices have appeared yet.
2. **Dock Ethernet.** Audit/include the existing FreeBSD-derived
   `PDFBSDem` I210 personality (currently omitted from the M1 boot set),
   including its DMA behind the new root. Require the physical I210 to
   attach, report link, and obtain an actual DHCP lease through pd-dhcp.
   Check routing/resolver interaction with existing Wi-Fi. Port Linux igb
   only if the existing em/I210 path proves insufficient.
3. **Dock USB.** Bind the existing PCI-capable Ravyn xHCI/IOUSBFamily stack
   to the actual controllers, with their correct PCIe DART/MSI and drain
   lifecycle. Then ask for a USB keyboard in the TS3 Plus **front USB-A
   rectangular port**; require enumeration and real typing. Follow with a
   test stick in that same port and an identity-checked read-only mount.
   Do not route these controllers through the direct USB1 DWC3 DART.
4. **Samsung through the dock.** Bind the real ACIO DP-IN tunnel/clock and
   shared crossbar to dcpext/DART/RTKit and the built compositor half.
   Require HPD, EDID/preferred modes, 3840x2160 scanout on the Samsung and
   the second connected connector/CRTC under `--outputs`, keeping the
   panel running. K1–K13 apply; framebuffer DMA and DCP/PHY teardown must
   drain before a tunnel or backing allocation can disappear.
5. **Dock hotplug.** After static functionality, request ten controlled
   unplug/replug cycles of the TS3 cable on left-front Type-C port 1,
   captured on UART: no panic, stale DMA, stuck link, lost internal panel
   or permanently missing Ethernet/USB/display after reconnect.
6. **Later hardware.** Retain direct USB2/3/DWC3, USB-C DP adapter, TS4 and
   Kensington source paths. Inventory/qualify each only when available;
   these no longer delay dock bring-up or cause dedicated boots now.

Each kernel install is one logical change on the verified base, using only
`install/linux/sf-bootbin-install.sh`, with rollback, two-minute coordination
announcement, gui.lock and M5 UART capture before its first boot. Default
boot remains Linux; return the shared machine to SiliconForest. Track current
state in `runs/m1-coord/status-CODEX-TB.md`. Sources/captures/identifiers stay
outside git. Ported GPL/MIT notices are retained per file and in kext NOTICE.


## Historical next implementation boundary (tc01; superseded above)

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

### Manager source dependencies and crypto, 2026-11-15 04:43Z

The pinned source set now includes USB4 port devices, dock quirks, SHA224/
256/HMAC, constant-time comparison, UUID/GUID helpers and Linux component
handling. The full SDK target verifies 27 objects (21 manager, four library,
two native), and no manager/USB4/crypto/UUID functions remain unresolved
between those objects. Kernel services and four transactional devres group
APIs remain required. In particular, the existing DCP group compatibility
functions report success without rolling back failed-bind resources; they
are not used as implementations here. No native aggregate binding or full
kext/link closure is claimed.

Full object build:
`/nix/store/ksda3m9pgimr6chc4600i8b6xnrimfjc-puredarwin-thunderbolt-cm-objects-0.1`.
Core build: `/nix/store/953fygchfy63lh2rqz7mc4g3hsihrajq-puredarwin-thunderbolt-cm-objects-0.1`.
The byte-exact source/header hashes and original notices are retained;
SDK/host preparation now shares `tools/prepare_cm.py`. It fixes an upstream
USB4 registration error path which otherwise unregisters/frees the device
and continues with PM calls. A host test compiles that actual staged add
function against the production native registry/device adapter: component
failure returns the error, frees once, restores parent references and makes
no later PM calls. Busless USB4 port children and separate wake capability/
wake policy are covered too.

ASan/UBSan also passes 1000 independent Python hashlib/hmac comparisons of
the unchanged C crypto bodies, including SHA224/256, both HMACs, empty and
long keys, padding/block boundaries, unaligned buffers, incremental updates
and context clearing. Comparison tests vary differing positions/lengths/
alignment; UUID tests cover order and malformed text. This is functional
and memory evidence, not timing/FIPS validation or dock authorization.
All six repository checks pass; failed initial compile receipts are kept.

USB04 remains installed, without a new boot or direct device attachment.
The TS3 is still deferred and no NHI/tunnel/external display milestone has
been demonstrated. Continuing resource-group rollback, shared native ATC/
PHY ownership, ACIO and external provider integration.

### Transactional component resources, 2026-11-15 04:49Z

The CM target now provides the four devres group APIs, using the pinned
Linux marker/rollback design on its private BSD LinuxKPI layout and newest-
first list. It retains the BSD file's notices and the Linux design's GPL/
SUSE/Tejun notices. Closed groups release only their bounded interval;
open groups roll back to their marker, in reverse allocation order. Marker
removal keeps resources attached; allocation failure leaves previous
resources alone. Callbacks run after list extraction and outside the lock.
Crossing closes are refused before list changes and fault the native
adapter. This implementation requires proper nesting, the subset used by
component.c. It does not establish DMA/IRQ/producer quiescence: callers
must stop and join those users before resource retirement.

ASan/UBSan tests run the actual group include with a host BSD-layout and
allocation fixture: 500 nested rollback/reverse-order cycles, closed/open
groups, removed markers, failure of each allocation, crossing-close refusal,
full teardown and callbacks which reenter allocation. The native PM suite
also refuses an invalid (>1) suspension-query result and still allows
metadata teardown after fault. All prior host tests and six repository
checks pass (274 Python, 77 C groups, six mutations).

The verified full target has 28 objects, including the same private
LinuxKPI devres unit a future kext layer must compile once. A future kext
must not link a second copy of that runtime. Four group APIs are now absent
from the remaining-external symbol list. Full SDK:
`/nix/store/mjlq420i01yl36dgx9r8gpcv8g5021c1-puredarwin-thunderbolt-cm-objects-0.1`.
Core-only (13 objects):
`/nix/store/c8sxr85bsh596fmaa4c7ws7ik1z53yf9-puredarwin-thunderbolt-cm-objects-0.1`.
This remains an object target, without native ACIO/PHY/PM/backend binding.

Read-only USB04 observation at 04:46:51Z still shows TS3-only Thunderbolt,
4422 samples, zero Type-C read errors, host inactive, alternate-mode deferred
and removal not blocked. No direct USB2/3 or DP flag is present. No new boot,
controller DMA, NHI traffic or external output is claimed. Continuing with
shared native ATC/PHY ownership and ACIO/external provider integration.


## Dock-first replanning and idle comparison (15 November, 05:44Z)

The acceptance order is now dock PCI functions, I210 link/DHCP, dock PCI
xHCI with physical keyboard/read-only test media, Samsung DP tunnel, then
ten dock-cable cycles. The existing em personality includes I210, and PCI
Ravyn xHCI exists; both still need PCIe-C DMA/MSI and lifetime auditing.
No dock function has yet enumerated in Darwin. Direct-host sources remain
useful for shared PHY ownership, but require no further direct-only boots.

One announced, UART-captured, gui.lock/bench-controlled k2/USB04 A/B restored
the exact USB04 kernel through the normal installer. Relevant userland hashes
were identical across the two boots. The original 120-second k2 probe hit
its 120-second wrapper timeout: that full run FAILED. Its recovered first
100 seconds are useful partial evidence; the restored USB04 90-second probe
completed. Kernel_task used 2.42% of one CPU on k2 and 1.94% on USB04.
The dominant thread was the same preexisting PDAppleNVMeRW polling worker
(1.83% versus 1.28%). Stackshot symbolication identified its timed lock sleep;
NVMe and all kernel modules except the USB/Type-C additions have identical
module UUIDs. No new NHI/controller DMA was running. Aggregate idle was
97.87%/98.26%; load near one also existed on k2. A governor-period test did
not remove the NVMe cost; its original setting was restored.

Warm fixed-work timing was 83.7 ms on both kernels; 100,000 getpid syscalls
took 14.66 ms on both. The reported AppKit 1.5–2x slowdown was not reproduced
by those probes and its cause is unresolved. NVMe's unchanged worker explains
the largest measured kernel idle cost, not that reported application slowdown.
No NVMe code, write gate or fence was changed. Avoid claiming a regression fix.

A separate read-only 20-second compositor sample found pdhid-reader CPU
0.361% of one core, main 3.496%, and Shm dropping thread 1.951%. Whole-task
interrupt wakeups were 8,819 (441/s), platform-idle wakeups 6,650; timer bins
2,565 and 4,218 are reported separately, not attributed solely to HID.
DESK5 confirms 1 ms active/2 ms intermediate/4 ms quiet HID polling because
the queue nodes have eno_select. This is a wakeup reduction opportunity;
it does not establish the AppKit slowdown's cause. DESK owns that input loop.
Its SIGSTOP finding is specific: minimal threaded/poll/read tests stop on
USB04, but the live compositor continues accumulating CPU after STOP.
The broader claim that all SiliconForest processes ignore STOP is unsupported.

Source hygiene removes the USB lifecycle's redundant 100 ms snapshot poll:
Type-C publishes a property-change message, the consumer queues a one-shot
on its own workloop, and stop closes admission before cancelling the timer.
The Type-C producer retains its slow 1 Hz observation. Both actual SDK kext
builds pass. This change is uninstalled and is not a claimed cause/fix; fold
it into the next dock kernel rather than spend a direct-only boot.


## Shared native PHY ownership for the dock (source only, 05:50Z)

The resumed broker work is now integrated with event-driven snapshot delivery.
An IOCommandGate serializes native TBT/USB4/DP claims and one-shot observation;
revoke callbacks run on a separate thread call outside the PHY gate. A changed
cable retains the owner's configured PHY until actual client drain and PHY stop
succeed. Token/epoch overflow closes admission; failed hardware cleanup keeps
the owner quarantined. No native NHI client is attached yet.

A checked, read-only PMGR extension rejects an unknown, active, clock-gated or
automatically waking ACIO1/M3 before PHY clocks/mux are changed. DWC3's stopped
view alone was insufficient to establish that prior ownership. The helper is
staged privately, without changing sound's pd_soc.cpp. Separate error properties
accompany aggregate ACIO and CPU gate values; zero raw data on an error is not
off evidence. A poisoned prior USB DART also prevents alternate-mode admission.
DP AUX partial-start cleanup and the upstream zero-rate no-op are preserved.

The native host bundle SDK-builds; ASan/UBSan covers 10,000 lease transitions,
actual PMGR refusal/parser paths and all existing ATC/DWC3/DMA lifetime suites.
This is not installed and proves no dock operation. Next is the native ACIO
RTKit/NHI attachment, its SID1 DART/interrupts and real CM/root-router discovery;
then apciec1 brings the dock's PCI endpoints into IOPCIFamily.


## Native ACIO1 service candidate (06:06Z, execution pending)

PDAppleCIO now links as a native kext. With `pdcio=1`, it consumes the shared
TBT/USB4 lease, refuses a bound apciec1 host/DART, resets that PCIe-C block,
powers ACIO and boots M3 using the existing M3v2 RTKit transport. It uses
runtime immutable tunables and only the validated ADT SRAM __DATA window.
It refuses RTKit's polling mode. One attempted startup per cable epoch and
producer epoch-change notifications avoid a retry/event-query busy loop.
RTKit/power-off errors preserve the PHY and resources rather than acknowledging
a false drain. Actual native PMGR-extension fault/timeout tests and SDK build
pass. Two early packaging builds failed (permissions/header staging); corrected
builds pass. No target execution is asserted in this paragraph.

The first collection candidate reuses USB04's frozen inputs, replacing only
Type-C and RavynXHCIPort and adding PDAppleCIO. UUID comparison preserves all
other modules, including internal DCP, GPU, NVMe and Wi-Fi; boot.bin reproduces
byte for byte. Common LinuxKPI weak USB imports remain unreachable in this
non-USB service, like the existing DCP/AGX copies: pd_fbsd_init only initializes
the required taskqueues and does not call the USB attachment helpers.

The firmware-stage properties deliberately distinguish RTKit-ready from
NHI-ready (false), domain/router/authorization and dock PCI enumeration.
A separate single-DART/SID1 native NHI DMA adapter compiles but is not activated
in this stage. Its root must be reprogrammed after ACIO power cycles; suspend
refuses outstanding mappings or a failed invalidation. NHI interrupts and
connection-manager attachment are the following implementation step, followed
by PCIe-C enumeration and the owner's prepared dock keyboard/LAN connection.


### CIO01 target observation (06:11Z): preflight failure, no RTKit start

CIO01 was installed normally and booted with internal desktop/Wi-Fi and the
standard .next/inject state restored. UART reader was stopped/reaped before
gui.lock release. Capsule SHA-256:
`c28cdae9f5fa019e77b09d03167f24c3a99ffedd616bc327f5fa8450a433e87d`.
PDAppleCIO matched, but returned ENODEV (19) before acquiring PHY; phy-owned
and quarantined are No, RTKit-ready/NHI-ready are No. This startup FAILED;
no M3/host-router/dock PCI progress is claimed. No panic was observed.

The CPU-compatible preflight used IODTMatchNubWithKeys, which calls the bare
node's name comparison rather than directly comparing its compatible data.
The CPU node is named acio-cpu1. Source now uses IODTCompareNubName and exposes
the current resource stage; this correction is not yet target-tested. Initial
ACIO gate state was not reached by that failed startup and remains unobserved.

Installed producer/consumer evidence: Type-C samples 61, snapshot checks 62;
consumer is event-driven, has one initial check, then follows the 1 Hz producer.
TBT-only state, zero Type-C errors, no active direct host or PHY lease persist.
The separate native NHI/DART interrupt resolver (24 actual ring IRQs + DART)
SDK-builds using the existing masked-AIC engine; not installed or registered.


### Preflight correction from actual ADT metadata (06:24Z; not installed)

Read-only IOKit XML verifies the M3 compatible bytes (19 including NUL),
__TEXT;__DATA names (14), SRAM metadata (64), and actual PMGR table. ACIO1's
power-gates are physical ATC1_CIO / CIO_PCIE / CIO_USB; the clock-gates list
also contains virtual CIO1_RECONFIG-V, with no parents or state register.
The prior source's rejection of all virtual clock gates would therefore
refuse every claim even after the name check was fixed. No such gate read
was reached in CIO01. The corrected off check uses all three physical power
gates at the qualified T8103 addresses; ATC1_CIO covers M3. It never treats a
virtual reset vote as an off reading. Reverse shutdown follows the same
physical list and checks every mapping before any write. PCIe-C reset targets
physical ATC1_PCIE, not either virtual clock vote or the shared AON parent.

Host tests execute those corrected native extensions, require power-gates
rather than clock-gates, and reject unknown/active/all-ones state. The native
client also keeps a lease token returned on a failed PHY setup, so cleanup
cannot lose an owner when the PHY provider retained a partial session.
CIO02 is the next announced dock startup boot. NHI DMA remains disabled in
this firmware-stage test; no direct USB/adapter boot is planned.


### CIO02 target FAILED; UART diagnosis and USB04 recovery (06:35Z)

CIO02 capsule:
`4433cfc7328b7e585a7150927962d89f030cc59a06298e5ca0042ca0d97e0af9`.
The boot panicked at 06:30:09 with ESR alignment fault. Symbolication of its
exact collection resolves kernel memmove+0xa4, caller apple_rtkit_memcpy+0x160,
then apple_rtkit_syslog_rx_log and the RTKit RX workqueue. The I/O source is
four modulo eight, copying a 16-byte SYSLOG context twelve bytes into SRAM.
LinuxKPI defines memcpy_fromio as ordinary memcpy, which issued an unaligned
device-memory access. This is a real native ACIO bug, not an NHI/DART fault.

The trace establishes progression through PHY/power startup into actual M3
SYSLOG reception. It does not establish completed RTKit-ready state, NHI up,
host-router discovery, any tunnel or dock PCI function. No such result is
claimed. The owned cycle was interrupted to recover promptly; M5 reset and
the normal installer restored USB04. Standard .next/inject desktop/Wi-Fi were
confirmed at 06:35:15; UART reader was stopped/reaped before locks released at
06:35:17. The main CIO02 run remains FAILED; recovery passed. Rollback Type-C
reads still report no errors and direct host inactive/alternate-mode deferred.
The event-driven consumer and broker are therefore uninstalled again.

The source fix ports Linux's aligned I/O-copy sequence, preserving the Kalray
GPL-2.0-only notice/provenance. Only normal RAM is passed to libc; all I/O
reads are byte or aligned 64-bit. ASan/UBSan instruments 263,168 alignment/
length combinations and the exact fault shape; ARM64 codegen confirms the
access widths. Native SDK/collection builds pass; target retry is pending.
Other USB04 modules remain frozen for that retry.

TERM6 separately reports an application-side idle CPU/RSS regression caused
by WindowServer patch 0124 discarding frames and Activity Monitor recreating
8 MB buffers. It installed correction 0129 at 06:26. That reported finding
is separate from the unchanged NVMe worker measured in the kernel A/B; it is
not a claim that our CPU-only probes established the AppKit timing cause or
that a USB kernel change fixed those application timings.


### Dock-first source progress and install transport (07:11Z)

CIO03 and CIO03B did not reach installation: the former's Linux upload was
refused; the latter's clean Linux transition did not answer. CIO03B captured
m1n1 through the next-stage handoff, without a captured panic. The external
reset recovery reached Linux and the normal installer confirmed unchanged
USB04; SiliconForest desktop returned at 07:04:24 and the owned reader was
reaped before lock release. This does not test the SRAM-copy fix. CIO03C is
retrying the same frozen capsule with one bounded external-reset fallback;
new source changes below are excluded from that capsule. Transport cause
remains unestablished; no Linux SSH/firewall/boot configuration was changed.

The private CM coherent/pool DMA adapter now requires real native allocation
and checked DART retirement callbacks. It accepts only ACIO1 SID1's qualified
IOVA window, rejects unsupported masks before allocation, zeroes payloads,
and retains failed/malformed mappings. Fault injection and eight concurrent
allocation threads pass under ASan/UBSan. Native PDCIODMABuffer has explicit
retire(), leaving its descriptor/backing owned when invalidation fails. The
DMA inventory is not a controller/IRQ/worker drain proof.

The Type-C source reads packed Intel VID status 0x59 for TBT, publishing its
raw device/enter VDO with status/data in one prefix-compatible immutable
snapshot. Missing cable fields refuse a TBT claim. Cable speed/active/link
training/generation/legacy changes revoke the shared lease. The current TS3
status implies 0x411 before the as-yet-unobserved legacy-adapter bit, rather
than guessing the cable value from USB flags. 128 protocol combinations,
invalid-state and v1/v2 parsing checks plus native Type-C/USB SDK builds pass.
This observer still runs at 1 Hz; no new periodic timer is added.

The native NHI bridge now SDK-compiles with the actual Linux ring engine and
software manager (30 full-target objects). It uses Apple's ring/PDF layout,
AIC callbacks, W1C status and throttle registers, and explicit native DMA.
It avoids generic Intel reset/interrupt setup, requires 12 pristine rings,
valid host DROM and Apple's VSE capability, unlocks ports and programs the
supplied cable value. PCIe/DP activation explicitly refuses until the actual
host backends exist. Private ring teardown checks VALID/IRQ readback before
clearing addresses, joins IRQ and worker before retirement, and quarantines
all mappings on failure. Domain removal owns its release; no second put is
issued. Native IORegistry/PM provider and combined kext linkage remain work
in progress; these compiled bridge objects are not an installed NHI or dock.


### CIO03D observed firmware-ready; next native CM build (07:34Z)

The unchanged CIO03 capsule was finally installed in CIO03D. Normal Linux
transition/upload/install succeeded without preinstall UART routing; capture
started after the install checkpoint. This supports avoiding the earlier
routing loop but does not prove its role in the refused/unreachable attempts.
The owner now separately authorizes removal of Linux SSH lock-outs at the
next normal visit, with no extra reboot; findings/changes will be in SF-R.

At 07:26:09 the M1 reports RTKit ready, firmware state 0x01000000, twelve NHI
rings and interrupt-enable zero, with stage error zero, no quarantine and
owned TBT PHY. The event-driven snapshot consumer made 62 checks for 61
Type-C samples. Standard desktop/Wi-Fi/inject are present; the owned UART
reader was stopped before releasing locks. This is actual M3 firmware-stage
success and hardware qualification of the SRAM-copy correction. It is not
NHI DMA, host/dock-router enumeration, authorization or a tunnel.

A 60-second read-only probe gives printed non-idle kernel-thread deltas of
2.70% of one core and aggregate CPU idle 97.83%. The dominant 2.08% worker
is symbolicated against unchanged module UUIDs as PDAppleNVMeRW's timed
lock sleep, the same worker identified in k2/USB04. Warm fixed work remains
83.7 ms and 100k syscalls 14.5 ms. The printed-thread threshold omits tiny
individual deltas; this is not a precise all-thread task CPU total. Load
average still around 1.1; the original AppKit timing regression is not claimed
resolved, and NVMe gates/fences were not changed.

The next source target links native IORegistry publication/final release,
actual runtime OS sysctls, native AIC/DART fault handling, and coherent DMA
callbacks into PDAppleCIO. Its cable-held power policy tracks usage/disable
state and records autosuspend hints without reporting hardware suspend.
The production policy passes failed-XDomain cleanup, 10,000 balanced router
sequences, suspend refusal, under/overflow and wake-policy host tests.
Raw ADT DROM bytes are copied at runtime into a private OF cache to bypass
LinuxKPI's numeric endian restoration; no DROM or identity is shipped.

The larger collection first exceeded the existing KC reservation. Only this
CM variant now dead-strips unreachable functions, with every BSD linker-set
member explicitly protected. Binary comparison retains all 29 SYSINIT and 11
SYSUNINIT targets in order, seven C++ initializers and the native metaclasses.
The collection fits its original address range and passes the overlap check;
boot.bin is unchanged. Generic compatibility UUID constants are privately
namespaced to avoid a duplicate symbol. The firmware-only build interface
is retained. Native CM is an explicit pdcio_cm=1 opt-in and has not run on M1.


### CIO04CM loader rejection; no NHI execution (07:47Z)

The first combined-driver capsule was installed, but native stage 2 rejected
its 619-byte command line before XNU: revision 2 permits fewer than 608 bytes.
This is a capsule-preflight mistake, not an NHI/DART result or kernel panic.
The owned cycle was interrupted promptly for guarded CIO03 recovery. Removing
only redundant pdrtkit_irq=2 (already the required native default) gives 605
bytes. The corrected capsule has exactly the same compressed collection and
boot.bin; the wrapper now checks the native limit before taking a test slot.
No NHI or dock operation is established by either capsule build.

During that normal Linux visit, the owner-authorized SSH lock-out change was
completed and eight rapid key-authenticated sessions passed. UFW remains on,
with both port-22 LIMIT rules replaced by ALLOW. PerSourcePenalties is off;
key-only authentication was explicitly enforced after discovering passwords
had been allowed. Exact before/after values, backup files and revert steps
are in SF-R's reaching-Linux note and the coordination log. No extra reboot
was used for this change; prior 9-second/60-second SSH workarounds are relaxed.


### CIO04CMB setup refusal and OS-info blocker (08:03Z)

The corrected 605-byte capsule booted without panic. Its extended 24-byte
Type-C snapshot is valid/error-free and derives TBT cable-info 0x411. M3
reported firmware ready, but CM creation returned a generic EIO before DART
preparation was logged. Cleanup completed: no retained PHY ownership or
quarantine. The guard restored CIO03 at 08:02:26; UART was reaped before
releasing locks at 08:02:28. NHI rings/CM and dock PCI remain unobserved.

Source inspection finds a definite OS-info blocker: XNU's kern.hostname MIB
lacks CTLFLAG_KERN, so kernel_sysctlbyname rejects that read with EPERM. The
other three OS MIBs permit kernel callers. Native creation previously hid the
underlying return as generic EIO; the target did not expose its precise stage.
The correction reads the actual exported hostname with a bounded copy (no
compatibility-layer homonym), retains real sysctls for the other fields, and
adds creation/start stage+errno and one-time power/ring/IRQ snapshots. No new
periodic poll is introduced. Native build/collection retry is in progress.

PCIe-C survey refinement from native DT plus the pinned Linux DTS: endpoint
DART is 0x521008000; raw ADT names mapper SID15, but this does not
establish Linux's endpoint domain assignment (see live-DT correction below).
It is separate from NHI's 0x501a80000/SID1. MSI base is
818/count32; the root interrupt is 850. Exact resource names come from DTS:
config 0x530000000, RC 0x520000000, port0 0x521000000, fabric 0x520004000,
debug 0x520200000. Linux additionally names LTSSM 0x521010000 and OE-fabric
0x520008000 beyond the five raw ADT entries. Informal PHY/port labels from the
earlier survey must not be used to map these windows. The pinned
`t8103-pciec-tunables.dtsi` supplies the additional OE-fabric configuration;
no captured calibration values are being added to source. Native endpoint
bridge/DMA and PCI xHCI/I210 binding are still unimplemented.


The pre-retry startup audit also catches the missing NHI tunable application:
pinned apple_nhi_probe applies apple,tunable-nhi before rings. The native
firmware stage loaded/validated those eight fabric/management groups but
intentionally did not apply them. The CM entry now requires HOP_COUNT 12,
IRQ enable zero and every ring VALID clear, then applies PD_CIO_NHI before
DART/rings/manager startup. This remains a source correction pending target
qualification, with no new timer and no effect on the firmware-only build.


Native failure closure is also wired through to actual DMA admission: a
faulted device/PM adapter now makes the native owner unavailable, rejects
new Linux ring enqueues before descriptor writes, and masks/quarantines a
ring that interrupts after the latch. Metadata teardown remains available;
a failed controller drain retains backing and the PHY lease. This closes a
source gap between the previously tested metadata fault latch and the new
native ring engine; it is pending hardware qualification with that engine.

### CIO05 hardware result (15 November, 08:23–24Z)

Capsule `6768dfa61566…`, source `7109a8243` + `6d50a5fe6`, boots the
internal desktop and passes actual native connection-manager creation
(`cm-create-stage=complete`, errno 0). All three CIO power gates read `0x3ff`;
NHI fabric tuning and DART preparation complete, and the control TX/RX
interrupts register. Domain startup then returns errno 12 alongside DART
fault `0x8f0f0001`. The pinned Linux decoder identifies SID15/no-TTBR; the
NHI device tree assigns SID1. This is evidence of a failed hardware DMA
startup, not dock enumeration. Fault admission closes, NHI readiness stays
false, and the owner retains PHY/power in quarantine. The guarded cycle
restored the known firmware-only CIO03 at 08:26:52Z; its UART reader was
reaped at 08:26:54Z. Next work captures DART state and
fault address and compares Linux before changing stream ownership.

The Linux SSH lock-out removal also persisted at this normal Linux visit;
see the exact before/after and rollback in SF-R. No further SSH policy edits
were made.

The follow-up diagnostic capsule `d3aeebc0f120…` adds bounded before/after/fault
DART register snapshots and control-ring DMA/configuration evidence. It changes
no stream assignments and adds no timer. The native SDK/collection build has
zero overlaps, the other 56 module UUIDs are unchanged from CIO05, boot.bin is
identical to CIO03, and its 605-byte command line fits the native 608-byte
capacity. All six required repository checks pass (274 Python tests, 77 C
groups, six mutation checks). Hardware outcome remains pending.

### PCIe-C port sequence, source only

`m1-thunderbolt/src/pd_pciec_port.c` now adapts the pinned Linux T8103 cold
reset table, all 64 RID2SID clears with readback, debug/fabric/port/OE/root
tuning order, bounded RUN/link-idle waits, and post-tunnel reset release and
LTSSM/link-up wait. Its access callbacks must enforce live ownership and
ordered nonposted root mappings; endpoint BAR mappings must be posted on
T8103. Failed accesses stop immediately, stale requester entries fail closed,
and timeout tests cover RUN, busy, tunnel reset and link-up under ASan/UBSan.
These are host register-model results. The source is not in the installed
kernel, has no IOPCIFamily bridge or SID15 endpoint DMA mapper, and does not
enumerate a dock. Cable reprobe/removal and ten-cycle evidence remain open.

### Live Linux PCIe-C topology correction (08:33Z)

The same normal install visit captured the public live Linux properties for
`pcie@530000000`: bus range 0..127, seven resource names
`config,rc,port0,fabric,debug,ltssm,oe-fabric`, requester mask `0x3f`, and
`iommu-map` mapping requester IDs 0..63 onto DART stream IDs 0..63. This
corrects the earlier inference from the native ADT mapper's SID15: Linux
does not assign all dock endpoints to SID15. The pinned `apple,t8103-usb4-dart`
variant supports 64 streams and places TTBRs at `0x400`, unlike NHI's
16-stream DART with TTBRs at `0x200`. The endpoint mapper must account for
that variant and the native bridge's actual requester assignments; the NHI
mapper cannot simply be reused. Linux still reports the authorized CalDigit
TS3 Plus as generation 3. Native endpoint enumeration remains absent.

The optional Linux MMIO snapshot was skipped because the NHI's runtime-PM
status did not establish active power; no unqualified MMIO reads occurred.
The public live-DT snapshot was saved privately.

### CIO06 diagnostic result and full-bank reset correction

CIO06 repeats the failure with no panic. Before preparation, every one of
the NHI DART's 16 TCRs reads `0x00080080` and every root is zero. The old
adapter changes only SID1 to its new root/TCR, leaving SID15 translating
with no root. The first fault is again `0x8f0f0001`, at VA `0x00100100`,
before the control-ring configuration diagnostic is reached. Native fault
admission closes and NHI readiness remains false. CIO03 was restored at
08:37:05Z, and the owned UART reader was reaped at 08:37:07Z.

The source correction follows the pinned Linux full unlocked-bank reset:
disable all 16 streams, clear all 64 TTBRs, enable streams globally with
isolation controlled by TCR, acknowledge pending errors, flush all streams,
then attach only SID1's root. It adds readback refusal for stale TCR/root/error
state. Failed reset/invalidation retains the bank and backing through native
stop, even if preparation failed before descriptor allocation. No SID15
mapping, bypass, PCIe-C DART write or polling timer is added. The production
reset helper is exercised by ASan/UBSan register-model tests covering all
access failures, stale SID15/root/error state, and retained TLB timeouts.
Native SDK/collection and all six repository checks pass for the final
correction (274 Python tests, 77 C groups, six mutation detections); the
collection has zero overlaps. Hardware qualification remains pending.

### Endpoint DMA audit before enabling I210

The existing `PDFBSDem` glue includes both `em` and `igb`. FreeBSD iflib
reaches `fbsdkpi/haiku/freebsd_network/bus_dma.cpp`, whose per-device tags
call `pdos_dma_map_phys`. The XNU seam in `fbsdkpi/xnu/pd_fbsd_os.cpp` already
uses `IOMapper::copyMapperForDevice` and `IODMACommand`; absence of a mapper
can still return a physical address. Its `pdos_dma_unmap` currently ignores
the result of `clearMemoryDescriptor(true)` and releases command/descriptor
ownership. Therefore an endpoint mapper alone is not lifetime qualification.
Before I210 binding, the tunnel path must refuse physical fallback and
retain DMA-owned backing on failed retirement, with actual device/IRQ/work
drain before DART or PHY shutdown. An owned bounce path can prevent a stale
translation from targeting recycled caller storage. This remains source
audit and a requirement, not an installed fix or Ethernet observation.

### CIO07 hardware qualification (08:48Z)

The reset correction starts the real software connection manager and host
router on M1 with error0 and quarantine clear. This removes the observed
CIO05/06 failure; no SID15 mapping or bypass was needed. Live Linux register
comparison during the same normal install visit confirms SID1 TCR `0x80`,
all unused TCRs zero, and TX/RX control flags `0xc0000000` with IRQ mask
`0x1001`. Native startup reaches the matching ring/IRQ state. No dock-router
child or PCI endpoint has appeared yet. Next work checks the trained link
and discovery before authorization and PCIe host activation. The guarded
60-second idle profile completes with NHI ready and quarantine clear throughout.
Printed non-idle kernel threads total 1.85% of one core (tiny omitted per-thread
deltas are not counted), aggregate CPU idle averages 98.43%, and warm fixed
work is 83.674–83.676ms for 50M operations and 14.620ms for 100k syscalls.
The dominant unnamed thread254 accounts for 1.24% of one core; previous
baselines identified thread254 as NVMe, but current-boot symbolization is
pending. This does not reproduce the reported AppKit slowdown and does not
claim its cause resolved. Current tool/userland hashes remain with the
private profile, not an assertion that every userland file matched the older
A/B. No added busy loop is evident in this NHI-up measurement.

### Next NHI link observation, source/SDK only

The native CM now has a bounded PHY-capability query through the real NHI
control channel. One separate one-shot timer, one second after successful
CM startup, samples physical port state and cached peer model IDs under the
domain lock and publishes them in `physical-port-state`. It exposes no UID,
DROM bytes or DMA addresses, performs no authorization or port configuration,
and never rearms itself. `link-query-count` records actual invocations; stop
cancels the timer before releasing resources. This distinguishes training/link
state from later router discovery without adding an idle polling loop.

Native SDK and both capacity checks pass, with zero collection overlaps. The
optional-AVE collection changes only CIO's UUID; the other 57 modules remain
identical to the frozen CIO07+AVE candidate. All six repository checks pass.
This diagnostic code is not installed; the active CIO07AVE cycle adds only
the requested lazy INFO module and preserves the qualified CIO07 NHI binary.

The next normal Linux visit observes a Keychron Q1 Pro `3434:0610` at USB
device `1-1`, including a HID boot-keyboard interface (class03/subclass01/
protocol01). The dock's four PCI xHCI endpoints and eight root hubs remain
visible in Linux. No additional owner keyboard action is needed; leave it
attached while the native PCIe path is brought up. This remains Linux
evidence, not Darwin USB enumeration or physical typing.

The native mapping audit confirms the XNU interfaces for PCIe-C ordering:
`kIOMapInhibitCache` selects Device-nGnRnE for device memory, while
`kIOMapPostedWrite` supplies Device-nGnRE. Root controller/configuration
accesses and tunneled endpoint BARs must use their appropriate distinct
settings, consistent with the pinned Linux T8103 warning. No endpoint BAR
has been mapped by this work yet.

The requested additive AVE INFO test used the exact frozen CIO07 collection
plus one diagnostic module, with all 57 existing UUIDs unchanged and zero
overlaps. The kernel and NHI boot passed, but root INFO returned ENODEV19
with flags0, caps0, soc8103 and no segments/identities. This is an early
inventory failure, before TEXT mapping; no DATA, AVE power/MMIO or encoder
was enabled. The older actual DT capture has a 256-byte padded
`firmware-version` property with the first NUL at byte16; the AVE reader's
whole-property-size guard rejects it before copying the bounded string.
The exact current failing property was not instrumented, so this source
blocker was relayed for the media agent to correct and qualify. The guarded
cycle restored NHI-qualified CIO07 at09:03:02Z and reaped its UART reader at
09:03:03Z. The next CIO08 link-diagnostic candidate omits this failed module
and changes only CIO against the qualified baseline.


### CIO08 observed, AVE paused, next PHY/link comparison (09:20Z)

CIO08 is installed and verified on the M1: NHI and the host router are ready,
RTKit is ready, and quarantine is clear. The once-only physical-port query
ran exactly once with error0. Both physical ports1/2 report state7
(UNPLUGGED), with no remote router. Dock authorization, tunneled PCI devices,
Ethernet, USB HID, and external display have **not** been observed on Darwin.
The next step is link training before router authorization and PCIe activation.

The owner paused AVE on2026-11-15. Source and its pause handoff are preserved
in SF-AD; all AVE requests are withdrawn. CIO08 is the known-good non-AVE
collection (capsule `c63179b7b9f2…`): manifest57/noAVE, live service/device-node
absent, no `pdave` argument. Installation verified09:08:35Z, UART reaped09:08:36Z.
No further AVE builds, boots, probes or firmware/kernel-image extraction.
The preceding paragraphs describing the AVE attempt and then-pending CIO08
are historical; these observations supersede their active-state wording.

CIO09 adds only bounded read-only diagnostics to the leased PHY/PIPE and
actual host-router Apple VSE cable word. The existing one-shot startup query
requests them while its owner/token and power proof are live; it never rearms
and makes no mode, rate, authorization or tunnel change. Native SDK/collection
build and all six required repository checks pass, with zero overlaps and
exactly the USB-provider and CIO module UUIDs changed against CIO08; other55
remain fixed and no AVE is included. This candidate is not yet installed.
The normal Linux installation visit will read matching ATC/PIPE offsets and
the active port1 host-router registers, guarded by a bound NHI, an authorized
up TS3 Plus and the actual I210 endpoint. Raw captures remain private.


### CIO09 observed comparison and next Type-C initialization (09:36Z)

CIO09 (`09c6b7c6b`, capsule `4c10751c6847…`) returned to the standard
SiliconForest desktop at09:31:33Z; the UART reader was reaped09:31:34Z.
NHI/RTKit remain ready, quarantine is clear, and the leased PHY query and
NHI link query succeeded exactly once. The collection has57 modules, noAVE
and no `pdave` argument. Both physical lanes still report UNPLUGGED/state7
and no remote router. No downstream dock or endpoint functionality is claimed.

The active Linux TS3/I210 capture and native diagnostic match all ten ATC
core registers, including lane/crossbar, power/reset, lane FSM and PLL clock
controls, and both read back Apple cable word `0x411`. PIPE mux/request/ack
also match. PIPE AON+0x1c differs in bit5, and NONSELECTED+0x20 in bit15.
These differences are observations, not a cause: the pinned Linux code
explicitly leaves dummy-PHY enable unchanged on T8103, and the AON bit is not
identified by that driver. No speculative write to either bit is planned.

An independent missing initialization step is now ported from the pinned
`cd321x_switch_power_state` and command helper: on port1 with APP firmware,
read SYSTEM_POWER_STATE; already-S0 performs no command; otherwise send the
one-byte S0 `SSPS` task and require its completion/result and actual S0
readback before publishing Type-C state. A busy foreign command is refused.
The one-second deadline plus iteration bound, sleeping between reads, and
no periodic retry prevent an idle command loop. Any error prevents PHY/ACIO
admission. Host ASan/UBSan exercises the actual helper, including every I/O
failure and frozen-clock bound. Native SDK/collection and all six repository checks pass, zero overlaps,
only TypeC UUID changed and other56 fixed; this step is not yet installed and is not claimed as the missing
link's cause. The next collection freezes every CIO09 module except TypeC.


### CIO10 S0 hardware observation; explicit late read candidate (09:44Z)

CIO10 is installed/verified09:44:03Z, UART reaped09:44:05Z. The real
CD321x power register starts at7, the bounded SSPS task completes in two
checks, and final readback is S0/0 with error0. NHI/RTKit and the internal
desktop remain healthy, quarantine clear. Both physical ports are still
UNPLUGGED/state7 with no remote router in the startup snapshot; no dock
router or endpoint appeared. Thus S0 initialization is a real missing step
now corrected, not a claim that the remaining link failure is fixed.

The next source/SDK candidate permits one explicit read-only snapshot via
`IORegistryEntrySetCFProperty`, using the actual calling-task administrator
check and existing CIO command gate. Only the one-key boolean
`pd-cio-read-snapshot=true` is accepted. No addresses, register writes,
authorization commands, timer or worker are added; the existing startup
snapshot still runs once, and subsequent reads require an explicit request.
Stop/admission/power/quarantine checks precede hardware access. The native
CLI also has a privilege-drop negative test requiring rejection and an
unchanged query count; a root request must complete and increment the count
exactly once. This will distinguish a later trained link from the early
startup state without ongoing polling. Native driver/tool/collection builds,
zero overlaps and all six repository checks pass; only CIO UUID changes
against CIO10, other56 fixed. Hardware qualification is pending.


The pinned PCIe-C DART probe explicitly skips parameter-register reads because
those reads can raise asynchronous SError during tunnel activation. Native
code must use the fixed T8020 USB4 topology (16KiB pages, IAS32/OAS36,
64 streams, TTBR+0x400, no bypass), the mapped register extent and ordered
nonposted access with completion barriers. The NHI's safe parameter reads
must never be copied onto this separate controller. The 64-stream global
reset also writes enable+0xfc and +0x100, the latter overlapping the TCR0
address in this register layout; its actual active Linux state needs to be
compared before adapting the bank-reset helper. No PCIe-C parameter probe or
DART write has been performed by this stream.


### CIO11 explicit read qualified, active Linux PCIe-C state (09:53Z)

CIO11 returned to the standard desktop09:53:22Z, reader reaped09:53:24Z.
The actual UID65534 request was rejected with NotPrivileged and count1->1;
root succeeded with count1->2. The late physical-port capability reads still
report UNPLUGGED/state7 on both lanes, error0 and no remote router. NHI/RTKit
remain ready, quarantine clear. The main block is link training, not merely
an early snapshot or a missing discovery event. Dock router/authorization,
PCI endpoints, Ethernet, USB and external display remain unobserved in Darwin.

During the same normal Linux visit, a bound controller and authorized active
TS3/I210 guarded the PCIe-C capture. A root-port UP check preceded ordered,
nonposted DART reads; parameter registers were excluded. The active64-stream
bank has TCR0/8/16/24/32 at `0x80`, others zero, no ERROR.FLAG, global enable
at+0xfc `0xffffffff` and+0x100 `0x80` (the latter is also TCR0). RID2SID
entries prove the five bus3..7/device0 endpoints use SID0 and intermediate
bus2/device1..4 bridges use SID8/16/24/32. Thus a native mapper must account
for bridge streams as well as the shared endpoint stream. Raw TTBRs and all
register captures remain private. No Darwin PCIe-C DART write has occurred.

The next USB-provider-only source candidate reads at most512 calibration
registers derived from the retained validated common/CIO lane groups, plus
the analog registers already used by fuse-derived setup. It preflights the
complete bounded list before reads, maps no new register window and never
reads the fuse controller or firmware storage. One snapshot runs after PHY
configuration with prior ACIO proved off and before client-lease commit;
the other runs through the existing live owner/token query. Independent
pre-ACIO/later counts and errors make partial results explicit. Linux will
read the matching live-DT register list under the established active-dock
guard, with masks/expected values kept private for comparison. Differences
may be consequences of firmware training and must not be blindly written
back. Native SDK/collection, ATC host suites and repository checks pass;
this extension is not installed yet. No timer or mode/rate write is added.


### CIO12 diagnostic failure and guarded restoration (10:06Z)

CIO12 booted with NHI/RTKit ready, no quarantine and the desktop running,
but both new calibration observations returned ENOENT2. The record-capture
qualification failed before the manual read. No panic occurred. The guard
restored qualified non-AVE CIO11 at10:06:38Z and reaped its UART reader at
10:06:39Z. The failed diagnostic remains a failure; no native calibration
record comparison has been obtained. Linux successfully captured261
register-list observations and the corresponding private expected masks.

Source audit finds the readback rejected a present zero-byte common group,
which the existing mapping/application contract permits. The correction
shares this presence/empty/lane validation between the real native adapter
and an ASan/UBSan regression test. Missing properties and empty lane tuning
remain failures. Group-specific failure metadata and empty-group reporting
will identify the actual table on the retry; the first failing table was
not instrumented, so it is not yet claimed as confirmed. Corrected native
SDK/collection, focused ATC tests and all six required checks pass; the retry
is not installed. No PHY configuration, rate or timer change is made.


### CIO12B readback qualified; next documented PIPE initialization (10:27Z)

CIO12B is installed and qualified10:18:06Z, UART reaped10:18:08Z. The empty
optional `tunable_CIO_CIO3PLL_TOP` is now identified, confirming the prior
ENOENT diagnostic cause. Pre-ACIO and leased reads both return250 records,
error0, with174 unique register addresses: the exact same address set as
Linux's261-list capture. Every native masked tuning value agrees with Linux's
expected configuration. Native pre-ACIO and late values are identical; six
full-register values differ from active Linux (offsets0x2088,0x2a28,0x9000,
0x90ac,0x10000,0x100ac). Raw masks and values remain private. This rules out
missing/misapplied retained tuning bits in the observed list, not all possible
PHY failures. The privileged read and actual nonprivileged rejection pass.
NHI/RTKit and the standard desktop are healthy; physical ports still report
UNPLUGGED/state7 with no remote router and no downstream devices.

The known PIPE difference at NONSELECTED_OVERRIDE bit15 has direct source
provenance: m1n1 `usb.c` at121fcbf35d45 writes the `0x9332` parked baseline
before Linux, including the documented DUMMY_PHY_EN bit. Native boot reaches
`0x1332`. The next TBT-only initialization enables that one bit and preserves
all other PIPE/AON fields. It requires a fresh physical ACIO-off check under
the exclusive uncommitted broker lease, a completed dummy park/ACK, and actual
DWC3 reset/clamp/native-reset/power-state readback. DWC3 reset is never
released. Invalid proof or failed readback aborts before NHI admission.
Already-enabled state sends no write. There is no new timer or retry loop.

The real helper's sanitizer tests cover refusal without writes, inaccessible
registers, missing stop/park/reset proof, failed enable readback, idempotence
and preservation of unrelated fields. Native SDK/collection and all six
repository checks pass, zero overlaps, only the USB-provider UUID changed
against CIO12B, other56 fixed. This functional step is not installed or
hardware-qualified yet; the other differing AON bit remains untouched.

### CIO13 observed: parked dummy PHY enabled, dock link still absent

At10:38:13Z the M1 booted the non-AVE CIO13 collection successfully; UART was
reaped at10:38:14Z. The guarded one-bit PIPE bootstrap completed with error0,
NONSELECTED override4914->37682 (bit15), and AON48 unchanged. The explicit
late root snapshot still reports both physical ports state7/remote0; no dock
router or PCI function appeared. NHI/RTKit, desktop/Wi-Fi, privileged query
and calibration checks passed. This corrects the documented parked-PHY
bootstrap difference; it does not establish a trained link. Next diagnostic:
bounded real NHI reads of lane-disable and adapter-lock bits.

### Native PCIe enumeration adapter and next lane diagnostic: source/SDK only

The new unwired `PDCIOPCIHost` compiles against the actual IOPCIFamily SDK.
It validates apciec1 resources, real physical RUN/link and retained owner
power, serializes/revokes config access, accepts subordinate multi-device
bridges and always strips command bus-master enable. No personality starts
it. Cold-init/tunnel callbacks, actual endpoint mapper/MSI, posted BAR mappings,
endpoint-personality exclusion and real child/DMA drain are still required.
No dock PCI or DMA evidence follows from this build.

The next read-only NHI diagnostic includes real LD (lane disabled) and LCK
(adapter locked) bits with flags-valid/error propagation. Its actual inline
reader passes ASan/UBSan input and both read-failure cases and64 field
combinations. SDK and all six local checks pass; this diagnostic is not yet
installed. It uses the existing startup/administrator query, with no recurring
timer or writes. CIO13 remains the observed non-AVE installed kernel above.

### PCIe-C native cold-init/link adapter: SDK and host evidence only

`PDCIOPCIController` maps the actual apciec1 root/controller windows and
uses nonposted accesses with dsb/isb ordering, actual ATC1_PCIE PMGR reads
and retained power/PHY-owner callbacks. It requires host/DART registry
children absent at initialization; it never touches PCIe-C DART parameters.
It adapts the existing bounded cold-init/link helper and43 published T8103
tuning records. All42 root records match the active Linux capture; the
remaining port record is the pinned t8103.dtsi constant. SDK and sanitizer
host tests pass (every tuning I/O failure, critical masks and idempotence).

This adapter remains unwired. No native cold-init/link or PCI enumeration
is claimed. Real CM tunnel callbacks/deferred activation, endpoint DART/MSI,
posted endpoint BARs and child drain are outstanding. Any attempted programming
pins its owner/mappings until reboot; revocation only blocks access and is
not hotplug teardown. CIO14's separately frozen diagnostic is the queued
hardware candidate; this controller is not in that collection.

### CIO14 observed: both lanes enabled and unlocked, still UNPLUGGED

At10:51:12Z CIO14 passed its actual native NHI/PHY/calibration, privilege,
standard desktop and Wi-Fi checks; UART was reaped at10:51:13Z. Both actual
NHI reads report PHY capability17, flags-valid1, lane-disabled0 and
adapter-locked0, yet state7/remote0. The same Linux visit reports capability17,
LD0/LCK0 and UP/state2 on both lanes. Disabled/locked adapters therefore do
not explain this particular link failure. No dock router/auth/PCI/Ethernet/
USB/DP has been established on Darwin. The installed collection remains
non-AVE,57 modules/no pdave/605 arguments, and excludes the newer unwired
PCIe controller adapter. Next source comparison: remaining cold PIPE AON
baseline difference; no register meaning is inferred from that difference.

### Next functional hypothesis: exact cold PIPE AON baseline

The public pinned m1n1 usb.c writes the whole AON word1; Linux atc.c then
clears DWC3 RESET_N and sets CLAMP, giving0x10. Darwin's observed cold/parked
word is0x30. Bit5's meaning remains unknown. A separate guarded experiment
accepts only these exact two words, with current ACIO-off and drained,
parked/reset/clamped DWC3 proof and dummy PHY enabled. It writes only the
held-reset word0x10, verifies readback, and rejects every other AON state.
It never releases reset. This is a cold-baseline hypothesis, not a claim
that bit5 explains training. The same bounded lease snapshot includes13
additional documented MAC/BIST/PLL registers for the Linux comparison,
without recurring polling or writes. CIO14 remains installed until a
separately validated and announced candidate is run.

### TS3 authorization and tunnel hooks: source only

An explicit native NHI authorization entry point and optional all-or-none
PCIe backend hooks now build in the SDK. There is no installed caller or
backend; absent hooks refuse authorization/activation. The real domain lock,
actual root/remote relation, direct03d:0011 model/route and fresh physical
UP read precede the actual CM approve call. Keyed, unplugged or ambiguous
peers are refused. Authorization/event publication occurs only after real
CM success. The bounded snapshot can report actual remote authorization.
The host allowlist test is metadata evidence only; no dock has been authorized
on Darwin. These source changes are excluded from the frozen CIO15 candidate.

The actual native apciec1 DT capture also confirms all33 available root/port
tuning records match the published native constants; the ten OE-fabric
records come from the pinned public DTS and match active Linux. Raw data
remains private. Linux runs cold-init from its deferred post-activation
worker, so both native coldInit and linkStart now require real activated-
tunnel proof outside the CM domain lock. The pre hook reserves prerequisites
without PCIe-C MMIO; no native backend is installed yet.

A native read-only PCI inventory CLI now SDK-builds for stage(a). It follows
actual IOPCIDevice ancestry to PDCIOPCIHost and prints only public PCI IDs/BDFs.
Acceptance requires all four xHCI controllers and the I210; root-only/partial
inventories fail acceptance. No native invocation or PCI hardware evidence
is claimed until it is run on the M1.

### CIO15 observed: cold AON hypothesis did not train the link

At11:04:51Z the actual CIO15 kernel passed NHI/PHY/calibration, root versus
unprivileged query and standard desktop/Wi-Fi checks; UART was reaped at
11:04:52Z. The exact guarded AON normalization completed48->16 with error0.
All23 expanded PHY/MAC/BIST/PLL registers match active Linux, as do the
five sampled PIPE registers. Both physical lanes remain enabled/unlocked
but UNPLUGGED/state7, remote0. Thus this cold AON change alone did not fix
training; bit5's function remains unknown. No dock router/authorization,
PCI, Ethernet, USB or DP has been observed on Darwin. The installed
collection has57 modules, no AVE module or pdave argument, and unchanged605
boot arguments. AVE remains paused.

The real native PCI inventory CLI ran on CIO15 and returned2 with
functions0/xhci0/i2100, correctly failing the stage(a) registry acceptance.
That negative execution qualifies the tool's empty-domain path only.
The positive four-controller/I210 path remains unobserved. No further reboot
will interrupt the announced Firefox30-minute stability slot; source/host
work proceeds on combined Type-C control/NHI lane diagnostics and endpoint
DMA preparation.

### Combined next diagnostic: source/SDK only, CIO15 remains installed

With23 PHY/MAC/PLL and five PIPE registers matching Linux but lanes still7,
the next read-only candidate batches the remaining control boundaries:
Type-C event1/mask1/power/status/data via a real serialized port1 control
reader, root/adapter/PHY headers over NHI, actual wrapping24-handler IRQ
counts, live M3/IRQ state and256 ACIO tuning register observations. Every
tuning address/count is preflighted before reads (capacity512); no firmware/
SRAM/DART parameters or caller-provided addresses are read. The native
Type-C helper has input and every-read failure tests; failed reads leave
output unchanged. Its query shares the existing sampler's workloop and is
rejected after stop. No IRQ acknowledge, mask/role/mode write or new recurring
poll is introduced. Source/SDK evidence remains separate from hardware.

The Linux comparison is prepared for the next ordinary visit: fixed port1
I2C_RDWR register reads (bus DT i2c@235010000/usb-pd@3f, actual bound driver,
J293), with the Linux adapter serializing each address-prefix/read transfer;
no I2C_SLAVE_FORCE, command task or IRQ/mask write. ACIO samples use the
same256 validated addresses and active-NHI/authorized-dock guard. Raw
snapshots remain private. No extra visit/boot interrupts FX's stability slot.

### I210 driver preparation: separate SDK artifact, not installed

The existing PureDarwin em/igb source includes I2108086:1533. A dedicated SDK
variant now builds it with its45 clean source files pinned and checked. Its
private fbsdkpi patch requires the actual native PCIe-C mapper, rejects physical
fallback and validates full owned spans within nonzero32-bit IOVA space.
The named mapper is not implemented/registered yet, so this is intentionally
not DMA admission. Transfers bounce through distinct page-owned descriptors;
failed prepare/segment/sync/retire keeps command/backing and poisons further
mapping. Posted PCI BAR mappings are confined to this Ethernet artifact;
the native root/config path stays nonposted. No tunnel-compatibility bypass
or boot-collection addition is made.

Exact frozen XNU source confirms clearMemoryDescriptor(true) discards errors
from complete(). The adapter calls/checks complete() explicitly and clears
only an inactive descriptor before releasing backing. The actual callback-
ordering helper passes failure/no-release sanitizer tests; this is not real
DART TLB or device/IRQ/work-drain proof. Those native mapper, MSI/teardown and
positive DMA/link/DHCP qualifications remain outstanding. The uninstalled
Ethernet driver cannot be the installed kernel's idle-load cause.

### CIO16 observed readback; failed desktop qualification and CIO15 recovery

Source2614eb5d7/capsule9b9baa004c1b… booted at11:42:05Z. The actual startup
snapshot completed once with NHI/RTKit ready and no quarantine. All256 ACIO
tuning registers, including full words and intended masks, match active
Linux. All five host-router header words also match. Both lanes still report
UNPLUGGED/state7, enabled/unlocked, no remote router. Lane basic-header DWORD4
differs at bit30; PHY control DWORD18 differs at bits5/18/21/26/28. These
include dynamic lane fields; no speculative write is justified by a diff.
Type-C event1 differs at bits0/1/8/10; mask1 at bits0/2/8/9/54/55/59. Power,
status and data-status match Linux. Raw snapshots and comparison remain private.

The75s desktop/inject check failed at11:43:21Z. The standard recovery restored
qualified CIO15 through the normal Linux installer, verified the desktop and
inject directory, then stopped/reaped the owned UART reader before releasing
gui.lock at11:45:56Z. No panic was captured. This is a failed collection
qualification, not a working CIO16 deployment. Its late/root/unprivileged
queries and planned60s idle/IRQ comparison did not run. Retained session logs
show no CIO16 compositor-start record; the UART has no first SF_SESSION marker.
The early launcher cause is unknown; capture did not include its process tree.
CIO16B repeats the identical image with that missing evidence and a bounded
255s session deadline; it changes no source, kernel module, argument or AVE state.

Pinned Linux initializes the CD321x interrupt mask and acknowledges EVENT1
through CLEAR1; native startup currently ensures S0 and observes state only.
The observed mask/event mismatch identifies missing lifecycle work, not proof
that this alone prevents physical training. A native implementation must
serialize reads/acknowledges, handle reset/setup and failure, and avoid enabling
an unhandled GPIO interrupt or adding a fast polling loop. T8103 also has
lstx_explicit=false: its Linux M3-control RMW is already reproduced; the literal
write3 used on newer T6030 is not a T8103 fix.

### Port1 event acknowledgement prepared; source/SDK only

A later Type-C variant follows Linux cd321x_interrupt ordering: read eight
EVENT1 bytes, skip writes on zero, otherwise write exactly the observed eight
bytes to CLEAR1, then sample negotiated status. It shares the existing1Hz
workloop and changes no interrupt mask or GPIO/AIC enable. Failed reads do
not acknowledge; ambiguous failed writes are counted separately and publish
an unavailable sample, never an invented unplug. Actual-helper sanitizers
exercise every one of64bits plus zero/read-error/write-error cases. The native
Type-C SDK and all six required checks pass. No hardware event-clearing or
link improvement is claimed. This variant is excluded from frozen CIO16B.
Linux's mask/setup-reset recovery remains outstanding; acknowledging events
alone does not establish that lifecycle or prove the source of the training
failure.

### CIO16B qualifies the identical image; actual idle/IRQ result

The same capsule repeated successfully at12:03:54Z, after its first boot request
at12:01:35Z. The75s process/launchd snapshot shows sf-session and the standard
.next compositor alive, with the inject directory present. All native control,
PHY, ACIO observation and UID65534/root denial/acceptance checks pass. The
post-idle query completes with count3/error0. Owned UART was stopped/reaped
at12:03:55Z before gui.lock release. The installed manifest has57modules and
no AVE; an actual registry/boot-argument check also finds no bound AVE service
or pdave argument. The failed first attempt stays recorded and unexplained.

During a quiet60s sample, visible nonidle kernel thread work totals2.201445%
of one CPU, aggregate host idle is98.1876%, the dominant unnamed thread uses
1.55995%, and PDM1SPIHID0.33516%. The24 NHI handler counters advance only9TX0
and9RX0, including diagnostic control traffic across the measured boundary;
other22 remain unchanged. No NHI interrupt storm is observed. The parser
excludes the exact frozen XNU TH_FLAGS_IDLE=0x2, rather than counting idle
threads as work. Warm50M integer-mix runs take83.676–83.678ms and100k system
calls14.496ms, matching earlier fixed-work baselines. The cold first round
was104.548ms and is not substituted for a warm comparison. The prior k2/USB04
A/B remains2.42%/1.94% one CPU with the same NVMe timed sleep. These results
still do not identify the reported AppKit slowdown or qualify an idle fix.

CIO17/source83786dc6e/capsulefcf8f72d697e… is built and announced for12:08Z
or later under the usual locks. Only Type-C changes from qualified CIO16B;
other56module UUIDs,605boot arguments and boot.bin remain fixed,57modules/noAVE,
zero overlap. It tests exact observed EVENT1/CLEAR1 acknowledgement on the
existing1Hz sampler, with no interrupt-mask/enable or role/mode write. Actual
acknowledgement count, error/uncertainty, unchanged mask, standard desktop and
NHI health must qualify before accepting it. CIO16B is the rollback. No event
clearing or link improvement from CIO17 has yet been observed.

### Native requester-map backend prepared, not wired or installed

The unwired PDCIOPCIController now reserves actual dynamic BDFs in the64entry
T8103 RID2SID table, using the pinned PCIe-C iommu mask0x3f. The native method
requires current activated-tunnel/power and physical RUN/link readbacks; an
all-ones read is rejected. The actual reservation helper checks an empty
hardware slot, reserves it before the write, checks full readback and poisons
on transport failure, stale occupied hardware or mismatched duplicate. No
slot is recycled or retargeted. Error outputs remain unchanged. Multiple
endpoints legitimately share SID0 as on Linux; this is not per-device DMA
isolation. Intermediate bridge device numbers produce the observed8/16/24/32.

Bounds, table capacity, shared SID, duplicate/no-rewrite, stale slot, every
read/write failure and readback corruption pass actual-helper sanitizers.
The native CM SDK and all six checks pass. This source backend is excluded
from frozen CIO17; no mapping register is changed on the M1. It supplies no
DART table, MSI, endpoint driver/BME admission or hot-remove drain proof.

### CD3217 startup-mask preparation, excluded from frozen CIO17

A later Type-C source variant implements the remaining tipd_init startup
MASK1 step after APP/S0 checks and before service/cable publication. It uses
exact Linux tipd_cd321x_data bits1/9/10 (plug/power-status/data-status), not
STATUS_UPDATE bit8 or the newer SN201202x DP-SID bit. It reads first and skips
the write when already correct; a changed mask requires one full64-bit write
and exact checked readback. Read failure, ambiguous write or mismatch prevents
PHY admission. Delivery still uses the existing1Hz polled EVENT1/CLEAR1 path;
no GPIO/AIC interrupt is enabled. Startup and acknowledgement have separate
properties, so the previous mask is preserved as evidence.

The actual inline helper passes already-correct/no-write, upper-byte clearing,
each I/O failure and readback-corruption sanitizer tests; the native Type-C
SDK and six required checks pass. This is not installed, is excluded from
frozen acknowledgement-only CIO17 and has no M1 mask-write/link evidence.
This startup step does not implement live controller-reset recovery or the
consumer teardown/revalidation that such a reset needs. Those remain required
before claiming the complete Type-C/hotplug lifecycle.

### CIO17 observed: event acknowledgement works, link still down

CIO17/source83786dc6e/capsulefcf8f72d697e… qualified at12:20:27Z, UART
stopped/reaped at12:20:29Z before lock release. The standard .next desktop,
inject directory, Wi-Fi, RTKit/NHI, shared PHY and root/UID65534 snapshot checks
pass. The actual Type-C acknowledgement count is1, errors0, uncertainty false,
EVENT1 reads zero and MASK1 is unchanged. Both lanes are still7/remote0. This
qualifies the native clear-event path and disproves acknowledgement alone as
the link fix. No dock router/auth/PCI/Ethernet/USB/DP observation is added.
The installed57module collection has no AVE; actual boot arguments have no
pdave. CIO18's startup-mask-only change is announced for12:25Z or later, with
qualified CIO17 rollback. Other56 UUIDs/605arguments/boot.bin are fixed.

The parallel quiet61s measurement reports nonidle kernel2.13797% of one CPU,
aggregate host98.29896% idle, SPIHID0.34219%, and18 NHI interrupts (9TX0/9RX0
including diagnostic traffic at the boundary; other22 unchanged). Warm50M
integer runs83.677–83.767ms and100k syscalls14.692ms match prior measurements.
The actual five-thread compositor uses3.48451% of one CPU: Shm dropping thread
2.10937%, main0.93908%, pdhid-reader0.43311%, agx-bo-cache0.00296%, disk worker0.
Actual TASK_POWER_INFO rates are interrupt428.197/s, platform-idle363.869/s,
timer bins115.393/s and206.918/s. The observer uses real SDK MIG/task ports,
checks the compositor PID and reads all13 thread/time samples; it sends no
input or signal. The named HID reader is not consuming a core. The cleanup
thread is the compositor's largest observed CPU consumer; this still does
not explain the reported1.5–2x AppKit slowdown. Findings were recorded for
DESK in the coordination answers/log; no desktop code or policy was changed.

### First-enumeration source prerequisites: isolated USB profile and DART blocker

An audit of the actual frozen bundles confirms RavynXHCIPort retains Generic
XHCI/IOPCIDevice matching class0c0330 alongside PDAppleUSB's native Type-C
personality. A new explicit build profile removes that PCI personality from
a copied USB bundle. Native build verification confirms only Generic XHCI is
removed; the five other personalities, top-level metadata and Mach-O bytes
are identical. It changes no installed bundle or default build and adds no
tunnel-compatibility/DMA flag. This is a first-enumeration profile ingredient,
not evidence of a safe complete collection or endpoint admission.

The unwired PCIe controller also gains blockEndpointDMA(), intended after real
activated-tunnel/RUN/link and before publishing the enumeration bridge. It
requires the actual DART DT node to have no bound clients, checks fixed bank
521008000/size, and rechecks physical power/RUN/link before every DART access.
An explicit register whitelist excludes PARAMS0/4. The actual helper checks
unlocked configuration/idle command, disables and verifies all64 TCRs before
clearing/verifying all256 TTBR words at0x400, selects both32-bit stream banks,
invalidates and requires bounded completion, then verifies all TCRs remain
zero. It never enables translation/bypass/global gates/IRQs or clears faults.
Any failed attempted operation poisons the retained owner; it proves no child,
IRQ/work or backing-memory drain. All712 modeled I/O failures plus lock,
readback and100us busy bounds pass sanitizers; the native CM SDK and six checks
pass. This code is not wired/installed and is excluded from CIO17/CIO18.
No PCIe-C DART register has been touched on Darwin by this preparation.
