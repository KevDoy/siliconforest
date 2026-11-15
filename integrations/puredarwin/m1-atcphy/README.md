# T8103 ATC PHY port

`src/pd_atc_usb2.c` directly ports the USB2 subset of Omacom Linux's ATC PHY
sequence with read/write/delay callbacks. It keeps both SuperSpeed lanes OFF
and PIPE on the dummy backend. It provides external reset/clamp, USB2 power,
common PHY power/status polling, the T8103 post-tunable sequence, orientation
routing and shutdown. A failed PIPE handoff prevents reset; failed power
polling prevents later stages. The caller must establish xHCI quiescence
before reset or shutdown. This code cannot infer that fact itself.

The kernel adapter is `../m1-usb-dwc3/kernel/PDAppleUSB.cpp`, built into the
same experimental xHCI bundle. It validates the J293 port-1 ADT layout and
complete compact common tunable bundle before PHY writes, reads port-1 fuse
fields using m1n1's original ordering, and applies common-a/AXI2AF/common-b
before the post-tunable sequence. Calibration values come from the running
machine; none are included here. Port 0 is reserved for the bench cable.

Host ASan/UBSan tests: `host-test/run.sh`. They exercise 100 power cycles over
both orientations, busy/failed PIPE handoffs, small/big power timeout,
tunable failure and shutdown ordering, plus 16,384 width/offset validation
cases. These are register models; no ATC writes have yet run on the M1.

The installed native adapter currently enables USB2 only. A direct USB3
device may use its USB2 fallback; this does not establish SuperSpeed.
The source backends below are separate from that installed adapter.

`src/pd_atc_dp.c` and `upstream/atc-t8103-dp.inc` now port DP AUX,
RBR/HBR/HBR2/HBR3 PLL/lane programming and the distinct T8103 DP-over-TB
pixel-clock sequence. `tools/extract_dp.py` keeps the upstream register
bodies with T8103 specialization and callback/serialization adapters. A
failed tunnel-clock shutdown retains ownership and blocks retuning until
shutdown succeeds; this strengthens upstream's discarded APB error.
The native caller must already own a powered, calibrated DP or TBT/USB4
mode and the PHY lock; these programs do not establish that ownership.
Host tests cover physical PMA masks, all rates, no-op repeated rates,
shared-PLL refusal and command/PLL/shutdown failures. `build-objects.nix`
SDK-builds the programs. No DP path is attached or hardware-tested yet.

`pd_atc_modes.c` ports the six T8103 mode/orientation programs: USB2,
USB3, USB3+DP, TBT3, USB4 and DP. TBT3 keeps PIPE on dummy; USB4 uses
its USB4 backend. The shared power program validates the lane bundle
before writes, applies runtime calibration after common tunables, then
configures AUX, lane routing and reset release. A failed calibration
prevents lane/reset release. This is an externally serialized backend;
the native owner must drain old clients and stop the shared tunnel clock.

`pd_atc_calibration.c` follows m1n1's runtime ADT lane-property grouping.
It preflights the complete borrowed, immutable bundle before applying any
record. USB3 selects four USB groups plus the other PMA's DP TX group;
DP selects both DP TX groups, and TBT/USB4 select both sets of four CIO
groups. Both orientations are covered. Calibration bytes remain on the
machine. Tests cover all twelve mode/orientation programs, validation and
application failure ordering, ten calibrated bundles and 56 missing
properties, changed programs and late malformed records. These source
backends SDK-compile; native calibration attachment and PIPE transitions
remain separate work. No SuperSpeed/DP/TBT hardware result is claimed.
