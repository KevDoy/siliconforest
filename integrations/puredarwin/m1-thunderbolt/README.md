# Apple Thunderbolt / USB4 port (SF-AF milestone 3, in progress)

`build-objects.nix` compiles the NHI/ACIO/DP-IN backend with the ARM64 SDK.
It is **objects only**, not a loaded driver, NHI, connection manager or
working tunnel. CIO02 panicked in an SRAM I/O copy. Corrected CIO03D is observed on M1
with RTKit ready, M3 state 0x01000000 and twelve inactive NHI rings. The
CIO05 reached native CM creation, DART setup and control-ring allocation, but
domain startup failed with DART fault `0x8f0f0001` (SID15/no-TTBR) and errno12.
The native fault latch quarantined the path; CIO03 was restored. NHI readiness
and successful control DMA remain unobserved.
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

Still required: qualify the implemented SID1 DART/ring DMA path and Linux NHI
control channel/software connection
manager and authorization; apciec1 tunneled host + MSI/DART/lifetime;
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
