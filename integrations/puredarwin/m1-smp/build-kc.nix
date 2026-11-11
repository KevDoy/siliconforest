# SPDX-License-Identifier: MIT
# The desktop collection (../m1-dcp/build-kc.nix: Wi-Fi + PDAgx + PDDcp by default here)
# with the two SMP pieces swapped in:
#   - PDArmPlatformExpert.kext from ./build-platform.nix (starts the secondary cores)
#   - the kernel from ./kernel.nix (AppleARMSMP names the reset vector)
# `--arg kernelPatched false` keeps ../m1-dcp/kernel.nix: the platform kext then only
# requires each core's reset vector register to hold a vector (true under the m1n1
# hypervisor, whose loader writes it).
# Each replacement fails the evaluation when its needle is not in the base recipe.
{ agx ? true, wifi ? true, slim ? true, kernelPatched ? true }:
let
  base = import ../m1-dcp/build-kc.nix { inherit agx wifi slim; };
  baseKernel = import ../m1-dcp/kernel.nix { };
  kernel = import ./kernel.nix { patched = kernelPatched; };
  platform = import ./build-platform.nix;
  platformNeedle = ''-kext "$KEXTS/PDArmPlatformExpert.kext"'';
  swap = needle: replacement: text:
    let out = builtins.replaceStrings [ needle ] [ replacement ] text;
    in assert needle == replacement || out != text; out;
in base.overrideAttrs (old: {
  buildPhase = swap "${baseKernel}" "${kernel}"
    (swap platformNeedle
      ''-kext "${platform}/System/Library/Extensions/PDArmPlatformExpert.kext"''
      old.buildPhase);
})
