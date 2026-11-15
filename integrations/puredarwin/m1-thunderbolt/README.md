# Apple Thunderbolt / USB4 port (SF-AF milestone 3, in progress)

`build-objects.nix` compiles the NHI/ACIO/DP-IN backend with the ARM64 SDK.
It is **objects only**, not a loaded driver, NHI, connection manager or
working tunnel. CIO02 panicked in an SRAM I/O copy. Corrected CIO03D is observed on M1
with RTKit ready, M3 state 0x01000000 and twelve inactive NHI rings. The
CIO05 reached native CM creation, DART setup and control-ring allocation, but
domain startup failed with DART fault `0x8f0f0001` (SID15/no-TTBR) and errno12.
The native fault latch quarantined the path; CIO03 was restored. NHI readiness
and successful control DMA were unobserved in CIO05. CIO07 now passes native
CM startup, NHI control DMA/IRQ and host-router publication after the full-bank
DART reset (`e6c674413`). TX0/RX0 flags `c0000000`, IRQ mask `1001`, error0,
quarantineNo. Dock-router discovery, PCIe/USB/Ethernet/display remain absent.
The owner-attached TS3 Plus remains a Thunderbolt-only Type-C cable flag in
Darwin; its USB/PCIe/Ethernet/display are not enumerated there.

`upstream/apple-dpin-*` preserves the pinned Linux lifecycle/token/request
code and its tests byte for byte (hashes in provenance.json). Its tokens
prevent old DCP callbacks from acquiring a replacement cable's route;
provider references span a successful attach until detach/callback drain.
The 35 original cases run through a host assertion adapter and minimal
OF/topology fixtures, covering 128,371 assertions. This is not Linux KUnit
execution or hardware qualification. The SDK target uses only pure state
code and portable scalar headers; host-only OF/KUnit/topology fixtures do
not provide fake kernel discovery, NHI or MMIO.

`src/pd_apple_nhi.c` ports Apple's ring layout/budget/PDF configuration and
NHI descriptor start/stop/encoding. VALID and E2E are programmed in one
write; disabled rings are checked before addresses clear. Errors retain the
running/mapping ownership flag. A caller holds the session/ring locks and
must supply a factual powered callback, owned ACIO-DART IOVAs and DMA cache
publication. Ring stop does not join workers or free memory. The backend
also checks RTKit SRAM IOVA bounds and implements the M1 DP-IN handshake
with one deadline covering HPD and INACTIVE_ACK. M1 does not use the M2's
experimental MODE_A/MODE_B handshake.

`src/pd_cio_lifecycle.c` ports startup/unwind order with mandatory backend
callbacks: configured PHY, unbound PCIe-C reset, power/reset, RTKit and
firmware readiness, tunables, children/domain and cable programming. Failed
child/RTKit/power drains retain the session and prohibit PHY power-off.
The separate native `PDAppleCIO` service now owns actual power, shared PHY,
RTKit and CM adapters. This portable helper's fixtures do not establish
hardware facts; CIO03 firmware readiness and CIO05 failure are recorded above.

Host ASan/UBSan checks add 64 ring/E2E cases, descriptor wire bytes and DMA
bounds, SRAM bounds, M1 DP-IN timing/error cases, failure at every startup
step, failed teardown retention and changed-cable full-OFF ordering.

Still required: dock router discovery and authorization; apciec1 tunneled
host + MSI/DART/lifetime;
PCIe-attached xHCI/Intel I210 (the existing em driver supports it), DP-IN /
dcpext tunnel binding and real dock hotplug. Never map or read ACIO children
before power/RTKit readiness, or after shutdown (the Linux driver documents
SError/watchdog resets on that ordering violation). Port 0 stays reserved.

The runtime ACIO calibration adapter decodes the distinct 24-byte ADT
records and maps all eleven RC/NHI/PCIe-adapter groups to m1n1's offsets.
It validates every record before writes, rechecks the entire retained
bundle on application, and preserves Linux's independent clear/OR-set
semantics and skip-unchanged behavior. ASan/UBSan tests cover all group
bases and 110 late malformed/missing-record refusals. SDK objects build.
No calibration bytes are included. Native RC/adapter tuning is firmware-stage
qualified; CIO05 also reached NHI tuning, then failed at DMA/domain startup.

`upstream/family/` and `pininc/` retain the pinned Linux software CM and its
protocol headers byte for byte; hashes are in `family-provenance.json`.
`build-cm-objects.nix` SDK-compiles twenty-one upstream manager files,
four upstream library files, two native device/registry objects and
the private LinuxKPI devres validation unit. `--arg full false` selects ten core
units plus the same adapters. The recipe verifies both the object count (28/13) and
domain object's presence; the inherited configure phase does not consume
`cmakeFlags`, so the option is passed explicitly. Earlier purported full
object results contained only the ten core files and are not full-build
proof. `nix-support/required-external` records symbols still needed after
subtracting definitions in these objects. This target neither links a kext
nor proves the complete kernel/native dependency closure (native ACIO/PHY/PM backends still need binding).

Staged-only adaptations disable trace instrumentation, guard domain init
until native callbacks are installed, and exclude the Intel x86 Apple PCI
resume-link function on ARM. Original source files retain their hashes.
The private LinuxKPI device layout extension preserves its BSD notices.
Wire helpers provide CRC32C and endian arrays; workqueue names are formatted
before creating a real ordered native queue. Byte properties use the real
fwnode readers. Completion dispatch avoids the SDK macro's collision with
the manager's `complete` member. Runtime-PM callback initializers are kept.

`src/pd_tb_registry.c` owns device and parent references and supports
concurrent traversal with removal/re-registration and a fixed watermark.
`native/pd_tb_device.c` implements bus/child discovery, registration rollback,
bounded uevents, release/type callbacks and parent ownership until the
child's final release. Structural operations require the native owner's
serialization; traversal invokes callbacks outside the registry lock.
A deleted child cannot be re-added while retaining its old parent. Optional
XDomain service-driver registration returns unsupported, without binding a
dummy driver. Router metadata removal establishes no hardware drain.

The native controller must install mandatory publication, event, real OS
snapshot and runtime-power callbacks once before domain init. Missing
callbacks defer initialization. Every runtime-PM call used by the CM bypasses
LinuxKPI's successful no-op helpers. A native power error latches the adapter
closed to new devices/power calls; metadata traversal and unpublication stay
available for cleanup. No native callback provider is installed yet, so this
code does not claim powered hardware or successful resume. A future NHI
owner must also stop new hardware operations when the adapter faults.

Host ASan/UBSan tests run the production registry and device adapter with
host-only reference/lock/publication fixtures: self-removing trees and final
parent lifetime, eight concurrent iterators, capacity and reference failure,
watermark/reverse traversal, partial-publication cleanup, bounded events,
and power failure followed by cleanup. These fixtures establish no actual
IORegistry, RTKit, DMA, controller power, authorization or tunnel facts.


USB4 port-device and dock-quirk bodies, SHA224/256/HMAC, constant-time memneq,
UUID/GUID helpers and the component framework now come directly from the
same pin (`upstream/lib/`, hashes in family-provenance.json). Fwnode byte
reads and zero-argument references use the actual native OF readers.
USB4 children can register without a bus under a registered parent; wake
capability and wake policy stay separate and both reach the real backend.
`tools/prepare_cm.py` applies the documented staged changes in both SDK
and host tests. It also returns immediately after failed USB4 component
registration; upstream otherwise continues after freeing the port device.
The host test compiles that exact staged add function and verifies the
error, final release, parent's references and absence of later PM calls.

The unchanged crypto/UUID source is tested under ASan/UBSan against 1000
independent Python hashlib/hmac cases, including empty data/keys, block and
padding boundaries, long keys, unaligned input/output, incremental HMAC and
context clearing. Memneq tests vary differing bytes/length/alignment and
UUID tests cover byte order and malformed text. This checks function results
and memory behavior, without claiming timing certification or dock auth.
No firmware write paths or device captures are enabled by these tests.

`native/pd_tb_devres_groups.inc` now supplies the four transactional resource
group APIs in this target's private BSD LinuxKPI devres unit, using the pinned
Linux marker/rollback design with the BSD list order/layout. Original notices
are retained; the full upstream design reference is in `devres-reference.c`.
Open/close markers bound a reverse-order rollback interval. Group removal
keeps resources attached. Failed marker allocation leaves earlier resources
alone; callbacks run after extraction and outside the lock. Invalid crossing
closes latch the native adapter closed before list changes. The supported
subset requires proper nesting, as used by component.c. Resource retirement
never establishes a hardware/IRQ/DMA drain; the caller must provide it first.

The object target includes the same patched devres source a future kext's
private layer must compile once. A kext must not link a second parallel copy
of that unit. The remaining-external manifest now excludes the four group
APIs. ASan/UBSan tests run the actual group include with a host-only BSD
layout/allocation fixture: 500 nested reverse-order rollback cycles, closed
and still-open groups, removed markers, every allocation failure, crossing
close refusal, full teardown and reentrant callbacks. Native PM tests also
reject out-of-contract suspension-query values. No native shared connector
aggregate, controller power, ACIO, DMA or tunnel binding is installed yet.


## Native ACIO1 firmware stage

`build-kext.nix` now builds PDAppleCIO, matching the port-1 shared PHY provider
when `pdcio=1`. It requests an exclusive TBT/USB4 lease, verifies that the
apciec1 host/DART have no service clients, resets that PCIe-C controller before
ACIO power-on, enables ACIO, performs the bounded T8103 CIO reset sequence,
and boots M3 through the existing interrupt-driven RTKit port. It validates
the immutable runtime tunables before any startup write. RTKit shared buffers
map only the ADT's validated SRAM __DATA segment, with checked IOVA bounds;
no firmware is copied or written. The 2 ms mailbox polling mode is refused.

A successful firmware stage reports `rtkit-ready`, raw M3 firmware state,
NHI HOP_COUNT and interrupt-enable state. It explicitly reports `nhi-ready`
false: there is no bound control channel/domain/router or dock PCI function
in this stage. Startup is attempted once per cable epoch, with no periodic
retry timer. The PHY producer notifies clients only on epoch changes, so a
query cannot generate an event/query loop. Revocation queues cleanup;
failed RTKit shutdown or power-off retains the handle, power and PHY lease.
No shared PMGR parent is powered down. The read-only gate check must pass
again before another initial PHY handoff.

The bundle also compiles a separate NHI-only PDCIODart/PDCIODMABuffer adapter
for `/arm-io/dart-acio1`, SID1. It is not activated by the firmware stage.
It retains checked map/unmap invalidation and quarantined backing; suspend
refuses outstanding mappings and retires translations before power-off,
while tables remain available for a later reprogrammed root. It must only
be called with NHI rings stopped and CPU workers joined. It is unrelated to
the USB paired DART and the future downstream PCIe-C mapper.

SDK kext and frozen-USB04 collection builds pass. The private collection
comparison leaves every original kernel/module UUID unchanged except Type-C
and RavynXHCIPort and adds PDAppleCIO; internal DCP, AGX, Wi-Fi and NVMe are
retained. Native power tests execute the actual extension with malformed
ADT/mapping injection, all-gate preflight, reverse shutdown and parent
retention after a timeout. All CM/PHY/DMA host suites and repository checks
pass. First target execution is still required; these are build/host facts.


CIO01 target execution matched the service but FAILED preflight with ENODEV
before a PHY claim; RTKit/NHI remained unready. The standard desktop/Wi-Fi
returned, and the capture reader was reaped. The CPU compatible check now
uses IODTCompareNubName for its bare device-tree node; stage diagnostics
identify future failures. This correction is awaiting the next target boot.

Read-only IOKit ADT inspection also confirmed physical power gates ATC1_CIO,
ATC1_CIO_PCIE and ATC1_CIO_USB. CIO1_RECONFIG-V in clock-gates is a virtual
reset command with no state/parent; it cannot establish a power state.
The PHY off check and reverse shutdown now use the three physical power-gates
and validate their known T8103 register addresses. PCIe-C's clock votes are
virtual too; reset targets physical ATC1_PCIE, with the known address and
active-state checks. Unknown/all-ones state refuses reset. No shared AON gate
or Wi-Fi domain is reset. The actual-extension host fixture distinguishes
power-gates from clock-gates and covers those refusals.

The next source stage adds an IRQ resolver for ACIO1's actual 24 ring sources
plus DART fault line, using a private 64-handle copy of the existing masked-AIC
engine. It is event-driven, caches handles across cable cycles and requires
mode 2 interrupt rearming; resolution itself does not register/unmask a line.
Failed PHY acquisition can still own a token after failed cleanup, so the
native client now retains that returned token and drain identity on error.


`native/pd_tb_dma.c` replaces physical-address coherent/pool DMA only for CM
objects. Its native callbacks must allocate through ACIO1 SID1 and explicitly
retire mappings before releasing backing. Unsupported masks, IOVA/pointer
bounds, partial backend failures and failed retirement are checked; faults
retain backing until reboot. Admission/counters alone prove no hardware drain.

`native/pd_tb_nhi.c` binds the Linux ring engine to Apple's layout/interrupt
operations and directly starts the software manager, avoiding generic Intel
reset/interrupt setup. It requires actual native IRQ, power and DMA callbacks
and native device/publication/PM installation first. PCIe/DP tunnel activation
is explicitly unsupported until host providers are bound. The private staged
ring engine verifies VALID/IRQ readback before address clearing and joins
IRQ/work before DMA retirement. The 30-object SDK target compiles this bridge;
it neither installs those callbacks nor establishes NHI or dock function.


`build-cm-kext.nix` now links the actual native IORegistry/PM/DART/IRQ callback
provider with the Linux manager. `pdcio_cm=1` opts in after the firmware stage.
It checks actual CIO power states, registers/join native AIC handlers, pins
controller power for the cable, and retires DART mappings explicitly. Runtime
OS strings come from real kernel state, avoiding the compatibility layer's
local osrelease string. XNU rejects kernel calls to kern.hostname because it
lacks CTLFLAG_KERN; the bounded actual hostname export is used for that field,
with kernel sysctls for the other OS values. Host DROM is a runtime-only raw-byte private OF property.

Only this larger variant dead-strips unreachable code after marking BSD
linker sets no-dead-strip. All 29 SYSINIT/11 SYSUNINIT targets and order, seven
C++ initializers and metaclasses are retained; the frozen collection now fits
its existing reservation. This does not change boot.bin or other modules.
The firmware-stage CIO03D is observed ready on M1; the combined CM kext is
built only. PCIe/DP hooks still refuse until native tunnel providers exist.

`src/pd_pciec_port.c` is a source-only T8103 PCIe-C cold-init and post-tunnel
link sequence. Its host test injects every I/O failure, a stale RID2SID entry,
and bounded RUN/busy/reset/link timeouts. Native ordered access, the PCI bridge,
endpoint DMA/MSI and cable teardown remain required; it is not linked into the
CIO06 diagnostic candidate. No endpoint-driver admission follows these helpers.


The next diagnostic candidate supports an explicit administrator-only
read request through `IORegistryEntrySetCFProperty` with the single key
`pd-cio-read-snapshot=true`. It runs the existing bounded NHI/leased-PHY
snapshot under the native command gate after admission/power checks, with no
new timer, worker, register write or authorization. `link-query-count` counts
the one startup query plus accepted explicit queries; errors remain in the
existing query properties. Build `build-snapshot-tool.nix` for the native
`pd-cio-snapshot` CLI; `--deny-unprivileged` drops credentials and requires
rejection with no count change. CIO11 qualifies this interface on the M1: UID65534 receives NotPrivileged
with count1->1; root succeeds with count1->2. The late real read still shows
both physical ports UNPLUGGED/state7 and no remote router. NHI/RTKit and the
standard desktop remain healthy, with quarantine clear. This is a diagnostic interface,
not evidence of a trained link or a dock router.

PCIe-C source preparation also includes `pd_pciec_config`: ECAM validation
accepts subordinate device numbers1..31 required by Thunderbolt bridges,
limits the root to device0/function0, checks aligned byte/word/dword accesses
against both bus and aperture bounds, and refuses a revoked view. A separate
command-write filter strips bus-master enable until the future native mapper
and lifetime adapter admit it. Sanitizer host tests cover full subordinate
BDF ranges, exact aperture edges, unaligned/overflow/refused views and command
widths. These helpers are **not linked into an installed host bridge** and
supply no physical power, synchronization, IRQ or DMA-drain proof. No native
PCI enumeration or endpoint-DMA protection is claimed from their tests.


`PDCIOPCIHost` now SDK-builds as an unwired IOPCIBridge subclass for
apciec1. It checks T8103 DT resources, an actual owner power callback and
physical RUN/link before configuration accesses; it serializes/revokes
configuration and uses the tested ECAM bounds and command BME filter.
All bus-master enables remain refused. It supplies neither endpoint DART,
MSI nor child-DMA drain. The future caller must retain real ownership and
omit endpoint personalities before enumeration; a registry marker alone
cannot establish those facts. It has no matching personality or automatic
start path. No PCI enumeration has been observed on Darwin.

The bounded NHI diagnostic also reads the actual lane capability's LD bit
and ADP_CS_4 LCK bit, alongside its state, under the existing domain lock.
An incomplete read leaves flags invalid and returns the error. Startup and
explicit administrator requests remain the only callers; no timer rearm,
write or authorization is added. The actual inline observer has sanitizer
host coverage for input rejection, both I/O failures and all64 state/LD/LCK
combinations; that is source evidence until the candidate boots.


`PDCIOPCIController` SDK-builds the native ordered-MMIO/power adapter for
`pd_pciec_port`. It checks real DT resources and absence of host/DART
children, retains the lease owner, checks the actual ATC1_PCIE PMGR state
before each access, and maps only root/controller windows with nonposted
ordering. It does not read any PCIe-C DART parameters. Its43 public T8103
tuning records derive from the pinned Linux DTS; all42 root records match
the active Linux capture (port record separately from t8103.dtsi).
The native adapter is unwired. Actual cold-init, tunnel activation, link,
DART/MSI and child enumeration are unqualified. Link startup must run outside
the CM domain lock; failures block later access and pin ownership until
reboot. Revoke is not a DMA/child drain implementation. No boot is scheduled
solely to run this adapter before a dock router exists.


The native NHI API now has optional all-or-none PCIe prepare/activated/
deactivate callbacks and an unwired explicit TS3 authorization entry point.
Absent callbacks refuse authorization and activation. Under the real domain
lock, authorization selects only the actual direct03d:0011 router on root
route1/2, checks a fresh UP state, rejects unplugged/keyed/ambiguous peers,
and invokes the pinned CM approve function before recording authorization
and emitting its real change event. Actual remote authorization is included
in the bounded snapshot when a remote exists. The model/route allowlist's
host tests prove only metadata matching; the SDK build is not an authorized
dock. No user-facing request or native backend is installed yet. Endpoint
DMA remains outside this API.


Linux populates/clocks PCIe-C only from its deferred post-activation worker.
The native coldInit API therefore also requires the actual activated-tunnel
proof and, like linkStart, must run outside the CM domain lock after its
post callback returns. The pre callback reserves ownership/prerequisites
without PCIe-C MMIO. This prevents a future caller from interpreting
"cold-init before reset release" as permission to touch a gated controller
before its real tunnel exists. The ordinary build leaves it unwired. The
explicit enumeration variant below connects it in source; it is uninstalled.

`build-pci-inventory.nix` builds a read-only native inventory CLI. It follows
actual IOPCIDevice ancestry to PDCIOPCIHost, prints only public BDF/vendor/
device/class values and counts the four xHCI controllers and I210. Exit0
requires that entire registry target set; absent/partial sets return2 and
incomplete queries return1. It never reads config space or proves Ethernet,
USB, DMA or authorization. Native execution is qualified separately.


The next diagnostic adds actual root five-DWORD and lane eight-DWORD basic
headers plus two-DWORD PHY capabilities, wrapping counts of the24 real NHI
IRQ handlers, current M3/IRQ registers and the256 native programmed ACIO
tuning addresses. Runtime tuning data stays private; only the prevalidated
owned address lists are sampled, under the existing power/lease gate, with
capacity512 and all-address preflight before reads. No SRAM, DART parameters,
firmware memory, user addresses, IRQ acknowledge or mode write is added by
these queries. The current startup and explicit root requests are the only
callers; handler counters add no sampling timer. A missing/failed Type-C
control snapshot makes the explicit diagnostic fail.

PDAppleTypeC's matching read-only control query samples event1, mask1, S0,
status and data status through its real port1 transport, serialized with its
existing1Hz sampler. Its command gate rejects calls after stop; it publishes
valid/error/count and invokes no task or IRQ-clear/mask write. These reads
are sequential observations, not an atomic snapshot of firmware activity.
Neither diagnostics nor host/SDK tests establish the dock link.


`build-ethernet-kext.nix` now SDK-builds the existing em/igb/I210 driver in
a separate, uninstalled variant. Its45 clean public source files are pinned
and checked byte-for-byte; the minimal DRM package had omitted this driver.
Only this variant patches fbsdkpi: every DMA path requires the actual
PDCIOPCIDartMapper (not yet implemented/registered), rejects identity fallback
and validates nonzero32-bit IOVAs for the entire owned span. DMA uses distinct
page-owned bounce descriptors, copies via the caller's original descriptor
only at setup/sync, and retains command/backing on prepare/segment/sync/
retirement failures. A poisoned device refuses new mappings. PCI BAR maps
use posted Device-nGnRE; root/config accesses remain in the separate native
nonposted host adapter. No tunnel-compatibility admission flag is added.

The exact frozen kernel's IODMACommand::clearMemoryDescriptor(true) discards
complete() errors. The owned path explicitly checks complete(), then clears
only an inactive descriptor, and releases backing only after both succeed.
The actual shared ordering helper's host tests inject callback failures;
they establish no device/IRQ/work drain or real DART TLB proof. Native mapper,
MSI/IRQ/work teardown, positive DMA and Ethernet link/DHCP remain required.
The Ethernet driver is not in any boot collection and no I210 has enumerated
on Darwin. These changes do not explain idle CPU in the installed kernel.

`build-enumeration-cm-kext.nix` is an explicit private first-inventory variant.
It installs the three real CM PCIe hooks, but the pre hook only reserves
ownership/mappings and the post hook only records that it ran. The administrator
request `pd-cio-authorize-ts3` calls the existing native direct-TS3 allowlist and
real `tb_domain_approve_switch`. Only after that function and its post callback
return and release the Linux domain lock does the native owner cold-initialize
PCIe-C, wait for link, block all64 endpoint DART streams and publish its native
IOPCIFamily host. NHI can continue its own traffic throughout these waits.
Failure blocks config accesses and retains resources. A programmed enumeration
host deliberately prevents CM stop/power release until reboot: real PCI child,
IRQ/work/TLB teardown is still required for hot-remove. A void Linux domain-stop
return is not accepted as proof of that drain.

This profile requires the actual IOCatalogue to contain only IOPCI2PCIBridge
personalities and the exact existing BCM4378 Wi-Fi match among IOPCIDevice
drivers. The guard inspects real retained catalogue dictionaries at startup,
before activation and before bridge publication; a marker is insufficient.
`build-enumeration-base.nix` copies an explicitly selected frozen base and removes
other PCI endpoint personalities. Actual artifact verification changes16plists,
preserves72 code files byte-for-byte and retains the three PCI bridge matches;
the separate frozen PDBwfm supplies the allowed Wi-Fi match. The USB profile
also removes Generic XHCI from the separate native USB bundle. Ordinary builds,
installed bundles and non-PCI personalities are unaffected. No tunnel-compatibility
or BME admission flag is added; the bridge always strips BME and the DART path
only blocks streams. The independent inventory CLI must still observe all four
xHCI controllers and I210 before stage(a) passes. These source/build results
provide no router, approval, tunnel, PCI enumeration or hot-remove evidence.

`build-authorize-tool.nix` builds the matching administrator CLI and a UID65534
denial mode. It accepts no arbitrary route/address/key. Its successful return
means the actual approval and native host startup returned successfully;
Ethernet, USB and even the complete PCI inventory are separate checks.

The explicit enumeration profile also has a one-per-boot administrator
`pd-cio-retrain-peerless` command, separately built by `build-retrain-tool.nix`.
Its real snapshot must show both port1 lanes unplugged, no remote and no PCI
resources reserved. It performs the existing NHI/IRQ/DMA/RTKit/PHY stop and
restart sequence; it is not a generic register interface or a polling retry.
Native SDK and policy checks pass; target drain/restart evidence is pending.
The `build-pci-dma-kext.nix` mapper variant is separate and uninstalled.


The explicit `build-pci-dma-kext.nix` and `build-pci-msi-kext.nix` variants
now include an administrator-only, single-attempt mapper factory.
`build-prepare-i210-mapper-tool.nix` builds its native request CLI and real
UID-denial control. It requires the live completed blocked PCI inventory,
finds the actual I210 under this host, starts the owned mapper, binds its
actual IOMapper object, and checks native lookup and readiness again. Partial
attempts remain retained until reboot; stop revokes and pins, without claiming
a child-DMA drain. BME stays blocked and no endpoint personality is added.
The ordinary and enumeration builds exclude this factory. All four SDK
variants and the CLI compile; no target mapper request or DMA is yet observed.


`build-dock-network-tools.nix` builds separate `pd-dock-dhcp` and
`pd-dock-netsetup` executables from the existing ISC DHCP source and pinned
native netsetup source. The generator checks the exact input hashes and
preserves their notices. It leaves shared Wi-Fi tools unchanged. Install
the two explicit tools under `/private/var/db/sftb/` before using the DHCP
client; there is no fallback to `/bin/netsetup`. Supply the actual I210
interface name: no default interface is accepted. The helper configures
only that interface, without loopback, default-route or resolver writes.
The lease client refuses pre-existing IPv4 configuration, scopes UDP
renewals with the real IP_BOUND_IF option, uses a separate lease filename,
and retires only a successfully applied address on daemon exit/lease loss.
An EEXIST address is refused rather than acquired for later cleanup. The
tools compile for the native SDK; target DHCP and cleanup remain untested.


`build-pci-ethernet-kext.nix` and `build-ethernet-admission-kext.nix` are a
separate, paired I210 network variant. They accept the exact I210 personality
in addition to the four inventory personalities; the ordinary/enum/mapper/
MSI builds retain their previous catalogue and BME policy. Initial Ethernet
matching refuses before a real native mapper exists. The explicit root
mapper request binds and verifies that object, then retries only this I210.
The private FreeBSD wrapper removes three early bus-master requests. After
real IRQ registration it uses a bounded attach-only wait for the actual
armed MSI route. Native host admission freshly validates mapper, topology,
physical power/link and the complete blocked inventory; it enables and reads
back BME on only the derived I210 forwarding bridges and endpoint, leaving
every xHCI blocked. No synthetic PCI domain, BDF, IRQ or DMA acknowledgment
is used. The CLI reports the kernel admission checkpoint separately from
independent interface/packet evidence. Ambiguous BME/startup failures retain
IRQ/context/pages until reboot. Both native SDKs compile; hardware Ethernet
link/DHCP and actual IRQ delivery still await testing. This is not hot-remove
support or a ten-cycle result.


The private I210 network variant uses IOPCIFamily's normal
`IOPCITunnelCompatible=true` declaration, required by its existing tunnel
matching policy. It retains that policy and never changes a device's actual
IOPCITunnelled state. The exact-I210 catalogue check requires this declaration.
Before FreeBSD attach, the native wrapper validates BAR0's real descriptor
against the T8103 PCIe-C non-prefetchable CPU window and binds a retained
native host/device access guard. Every private FreeBSD scalar/bulk BAR read
and write checks that real host's live power/RUN/link/admission state; bulk
operations use the scalar primitives. These guards and retained failure
contexts are implemented and SDK-built; physical removal remains unqualified.
The dedicated driver retains the original I210 initializer, rejects other
MAC types, retains bootstrap/driver linker-set entries during dead stripping,
and compiles only this private bundle with -Oz for collection capacity.

`tools/compact_kc_prelink.py` removes structural XML whitespace from our
generated __PRELINK_INFO, preserves all plist values and original tags,
and shrinks only that segment's extents and section size. It validates
metadata/payload separation and checks every other file byte is unchanged.
The usual overlap gate still runs afterward. This recovers metadata space
without moving the kernel or changing boot.bin. The candidate additionally
checks all57 input bundle non-debug sections against their complete original
SDKs after local-symbol stripping; this is build verification, not boot proof.
