# SPDX-License-Identifier: MIT
# The default desktop collection (../m1-cpufreq/build-kc.nix) with the kernel of ./kernel.nix
# in place of ../m1-smp/kernel.nix; every kext is the same build.
#   nix-build build-kc.nix --argstr kernelConfig development --no-out-link
{ kernelConfig ? "debug" }:
let
  base = import ../m1-cpufreq/build-kc.nix { inherit kernelConfig; };
  from = import ../m1-smp/kernel.nix { inherit kernelConfig; };
  to = import ./kernel.nix { inherit kernelConfig; };
in base.overrideAttrs (old:
  let phase = builtins.replaceStrings [ "${from}" ] [ "${to}" ] old.buildPhase;
  in assert phase != old.buildPhase; { buildPhase = phase; })
