# J293 Type-C port (SF-AF)

First step of the Linux CD321x/ATC/DWC3 port. `PDAppleTypeC.kext` reuses
m1-sound's Linux PA Semi I2C port and SoC power/mapping helpers. It samples
only CD321x **port 1**, one second apart, and publishes raw status and decoded
connection bits in IOKit. It is inert without `pdtypec=1`, and requires T8103,
J293 and the expected preserved HPM node/address. The installed loader prunes
port 0 to protect the serial cable.

This provider reports the negotiated Type-C state; PHY/DWC3/ACIO consumers
own their respective hardware startup. Published USB/TB/DP bits alone do not
establish a working host, tunnel or display.

The provider includes Linux's initial S0 check on the preserved
port1. It requires APP firmware, reads SYSTEM_POWER_STATE and sends `SSPS`
with a one-byte S0 payload only when the controller is not already in S0.
It refuses a busy foreign command, bounds completion by one second and an
iteration cap, sleeps between reads, handles rejection/timeout, and requires
an actual S0 readback before publishing a service or cable snapshot. It makes
no periodic command retry. No role swap, mux write, interrupt acknowledge,
firmware update or transaction to port0 is added. The command transport uses
the existing serialized I2C engine and Linux's length-prefixed framing.
CIO10 hardware qualification observes initial state7, SSPS completion in two
checks, final S0 and error0. NHI and the desktop remain healthy; the dock
router is still absent, so this is not a complete link fix.

Build: `nix-build build-kext.nix --no-out-link`. The experimental collection
is `build-kc.nix`; it retains the disk/reboot base and adds one kext. Pack
with `SF_KC=<collection> SF_EXTRA_BOOTARGS=pdtypec=1
../m1-native-boot/install/build.sh <private-output>`. Never overwrite the
shared install/out; install/reboot follows SF-AF and coordination RULES.

Host test: `host-test/run.sh` checks the actual block decoder's length/error
semantics, little-endian parsing and truncation boundaries under sanitizers.
A second test exercises the actual S0 helper with callback hardware models:
already-S0 sends no command; S3/S4/S5, every I/O failure, foreign busy and
invalid commands, rejection, failed final readback, deadline and stalled-clock
bounds. These host models do not establish CD321x hardware support. Kext lifetime:
timer callback runs on its workloop, samples serialize on the bus lock;
stop cancels/removes the timer before mappings and locks are freed.

Kernel clients bind to the `PDAppleTypeC` service and use `copyProperty` on
`port-state` (`pd_cd321x_snapshot`) to obtain a complete status/data/error
sample. Separate display properties may change during a sample and are only
diagnostics. A sample error must not be interpreted as a cable disconnect.
`pd-data-role-host` is the PD role; DWC3's `usb-role` follows DATA_STATUS's
USB connection/role bits. A Thunderbolt-only cable has USB role `none`.

See [SF-AF](../../../docs/roadmap/issues/SF-AF-usbc-thunderbolt-display.md)
for the surveyed chain and hardware evidence. GPL notices: NOTICE and each
source file. Imported code is referenced at build time, never read from
another Linux fork.
