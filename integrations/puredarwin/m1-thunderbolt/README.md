# Apple Thunderbolt / USB4 port (SF-AF milestone 3, in progress)

`build-objects.nix` compiles the NHI/ACIO/DP-IN backend with the ARM64 SDK.
It is **objects only**, not a loaded driver, NHI, connection manager or
working tunnel. No ACIO registers have been accessed by this port on M1.
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
These callbacks are not implemented by a native ACIO service yet; success
from a fixture does not establish any of those hardware facts.

Host ASan/UBSan checks add 64 ring/E2E cases, descriptor wire bytes and DMA
bounds, SRAM bounds, M1 DP-IN timing/error cases, failure at every startup
step, failed teardown retention and changed-cable full-OFF ordering.

Still required: ATC TBT/USB4 mode/calibration and ACIO reset provider; native
M3 mailbox/RTKit parent and power ownership; ACIO tunables and runtime DROM;
SID1 DART DMA mappings; Linux NHI control channel/software connection
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
No calibration bytes are included, and no native ACIO callback is wired yet.

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
