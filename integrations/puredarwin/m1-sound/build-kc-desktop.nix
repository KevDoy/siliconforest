# SPDX-License-Identifier: MIT
# The proposed default desktop collection with sound: ./build-kc.nix (the default desktop
# collection plus PDAppleSound, debug maps stripped) with the quiet-boot kernel of
# ../m1-quiet-boot/kernel.nix in place of ../m1-smp/kernel.nix, exactly as
# ../m1-quiet-boot/build-kc.nix does it for the collection without sound.
#   nix-build build-kc-desktop.nix --argstr kernelConfig development --no-out-link
{ kernelConfig ? "development" }:
let
  base = import ./build-kc.nix { inherit kernelConfig; };
  from = import ../m1-smp/kernel.nix { inherit kernelConfig; };
  to = import ../m1-quiet-boot/kernel.nix { inherit kernelConfig; };
in base.overrideAttrs (old:
  let phase = builtins.replaceStrings [ "${from}" ] [ "${to}" ] old.buildPhase;
  in assert phase != old.buildPhase; { buildPhase = phase; })
