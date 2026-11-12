# SPDX-License-Identifier: MIT
# The default desktop collection (../m1-touchbar/build-kc.nix: quiet-boot kernel, eight
# cores, Wi-Fi, GPU, display, sound, Touch Bar) with the kernel of ./kernel.nix in place of
# ../m1-quiet-boot/kernel.nix; every kext is the same build.
#   nix-build build-kc.nix --no-out-link
# Boot-args and launcher patch that go with it: see ./kernel.nix.
{ kernelConfig ? "development" }:
let
  base = import ../m1-touchbar/build-kc.nix { inherit kernelConfig; };
  from = import ../m1-quiet-boot/kernel.nix { inherit kernelConfig; };
  to = import ./kernel.nix { inherit kernelConfig; };
in base.overrideAttrs (old:
  let phase = builtins.replaceStrings [ "${from}" ] [ "${to}" ] old.buildPhase;
  in assert phase != old.buildPhase; { buildPhase = phase; })
