# SPDX-License-Identifier: MIT
# PDArmPlatformExpert.kext as ../m1-bringup-platform/build-platform.nix builds it (the
# baseline + its platform.patch), with ./platform-patches on top.
let
  pkgs = (import ../m1-rust-kext/pins.nix).pkgs;
in
(import ../m1-bringup-platform/build-platform.nix).overrideAttrs (old: {
  pname = "puredarwin-m1-smp-platform";
  src = pkgs.applyPatches {
    name = "puredarwin-m1-smp-platform-source";
    src = old.src;
    patches = [
      ./platform-patches/0001-PDAppleBootCPUPMGR-start-secondary-cores.patch
      ./platform-patches/0002-PDPerfControl-apple-start-after-all-cpus-initialized.patch
      ./platform-patches/0003-PDAppleBootCPUPMGR-accept-core-and-cluster-power-down.patch
    ];
  };
})
