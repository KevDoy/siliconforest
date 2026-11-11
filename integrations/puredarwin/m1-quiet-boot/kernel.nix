# SPDX-License-Identifier: MIT
# The lab kernel (../m1-smp/kernel.nix) with ./kernel-patches on top, both for the quiet
# boot: -noprogress also refuses the progress meter user space asks for, and a graphics boot
# sends the early log to the UART as a text boot does.
# `--arg patched false` is ../m1-smp/kernel.nix unchanged. kernelConfig as there.
{ patched ? true, kernelConfig ? "debug" }:
let base = import ../m1-smp/kernel.nix { inherit kernelConfig; };
in if !patched then base else base.overrideAttrs (old: {
  patches = (old.patches or [ ]) ++ [
    ./kernel-patches/0001-xnu-noprogress-refuses-user-progress-meter.patch
    ./kernel-patches/0002-xnu-graphics-boot-replays-early-log-to-uart.patch
  ];
})
