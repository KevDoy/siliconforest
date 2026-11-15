# J293 Type-C port (SF-AF)

First step of the Linux CD321x/ATC/DWC3 port. `PDAppleTypeC.kext` reuses
m1-sound's Linux PA Semi I2C port and SoC power/mapping helpers. It samples
only CD321x **port 1**, one second apart, and publishes raw status and decoded
connection bits in IOKit. It is inert without `pdtypec=1`, and requires T8103,
J293 and the expected preserved HPM node/address. The installed loader prunes
port 0 to protect the serial cable.

This is **not USB host support**. There is no Type-C setup, role/mux change,
ATC PHY, USB DART, DWC3 or xHCI start yet. USB/TB/DP properties describe what
the CD321x reports; they do not mean the corresponding host path is running.
No PD controller writes/4CCs/IRQ clears are issued. I2C0's PMGR clock and
FIFO setup are written by the shared transport; this is not a read-only MMIO
probe. Only register-index writes followed by reads are sent to HPM1.

Build: `nix-build build-kext.nix --no-out-link`. The experimental collection
is `build-kc.nix`; it retains the disk/reboot base and adds one kext. Pack
with `SF_KC=<collection> SF_EXTRA_BOOTARGS=pdtypec=1
../m1-native-boot/install/build.sh <private-output>`. Never overwrite the
shared install/out; install/reboot follows SF-AF and coordination RULES.

Host test: `host-test/run.sh` checks the actual block decoder's length/error
semantics, little-endian parsing and truncation boundaries under sanitizers.
It does not emulate CD321x or establish hardware support. Kext lifetime:
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
