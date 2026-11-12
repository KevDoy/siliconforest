# SPDX-License-Identifier: MIT
# The default desktop kernel (../m1-boot-speed/kernel.nix) with ./kernel-patches on top: the
# kernel never takes Apple's "set recovery variables in NVRAM and restart" path.
{ patched ? true, kernelConfig ? "development" }:
let base = import ../m1-boot-speed/kernel.nix { inherit kernelConfig; };
in if !patched then base else base.overrideAttrs (old: {
  patches = (old.patches or [ ]) ++ [
    ./kernel-patches/0001-xnu-IOSetRecoveryBoot-does-nothing.patch
  ];
})
