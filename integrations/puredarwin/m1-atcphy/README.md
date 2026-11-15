# T8103 ATC PHY port: USB2 lifecycle

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

This is USB2 only. SuperSpeed lane calibration and PIPE routing, DP AUX/link
rates/drive levels and Thunderbolt/USB4 mode remain to be ported. A direct
USB3 device may use its USB2 fallback; this does not establish SuperSpeed.

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
