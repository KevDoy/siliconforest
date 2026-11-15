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
