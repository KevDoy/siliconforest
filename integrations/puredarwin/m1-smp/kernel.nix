# SPDX-License-Identifier: MIT
# The lab kernel (../m1-dcp/kernel.nix: the pinned T8103 bring-up kernel + its patches) with
# ./kernel-patches on top. `--arg patched false` is ../m1-dcp/kernel.nix unchanged.
# `--argstr kernelConfig development`: see ../m1-dcp/kernel.nix. DEBUG is the default.
{ patched ? true, kernelConfig ? "debug" }:
let base = import ../m1-dcp/kernel.nix { inherit kernelConfig; };
in if !patched then base else base.overrideAttrs (old: {
  patches = (old.patches or [ ]) ++ [
    ./kernel-patches/0001-xnu-applearmsmp-pass-reset-vector-to-iopmgr.patch
    ./kernel-patches/0002-xnu-cpu-init-midr-line-uses-kprintf.patch
    ./kernel-patches/0003-xnu-park-offline-cores-idle-accounting-deep-idle.patch
  ];
})
