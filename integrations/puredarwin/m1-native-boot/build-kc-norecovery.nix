# SPDX-License-Identifier: MIT
# ./build-kc.nix (the native-boot collection) with the kernel of ./kernel.nix in place of
# ../m1-boot-speed/kernel.nix; every kext is the same build. For every boot that could, by
# a mistake in what is handed to the kernel, come up without its command line.
#   nix-build build-kc-norecovery.nix --no-out-link
{ kernelConfig ? "development" }:
let
  base = import ./build-kc.nix { inherit kernelConfig; };
  from = import ../m1-boot-speed/kernel.nix { inherit kernelConfig; };
  to = import ./kernel.nix { inherit kernelConfig; };
in base.overrideAttrs (old:
  let phase = builtins.replaceStrings [ "${from}" ] [ "${to}" ] old.buildPhase;
  in assert phase != old.buildPhase; { buildPhase = phase; })
