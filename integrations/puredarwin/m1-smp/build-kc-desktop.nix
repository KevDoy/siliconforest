# SPDX-License-Identifier: MIT
# The whole desktop collection: Wi-Fi + PDAgx + PDDcp (with the backlight interface) + the
# multitouch trackpad (../m1-trackpad-child/multitouch/build-kc-multitouch.nix), with the
# two SMP pieces of ./build-kc.nix swapped in (platform kext that starts the secondary
# cores, kernel that names the reset vector) and corecrypto.kext from ./build-corecrypto.nix
# (the per-cpu generator states overran each other). One CPU unless the boot-args carry cpus=N.
# `--argstr kernelConfig development`: the kernel of that XNU configuration
# (../m1-dcp/kernel.nix); the kexts are the same builds, see ../m1-dcp/README.md.
{ agx ? true, wifi ? true, slim ? true, kernelPatched ? true, kernelConfig ? "debug" }:
let
  base = import ../m1-trackpad-child/multitouch/build-kc-multitouch.nix { inherit agx wifi slim; };
  baseKernel = import ../m1-dcp/kernel.nix { };
  kernel = import ./kernel.nix { patched = kernelPatched; inherit kernelConfig; };
  platform = import ./build-platform.nix;
  platformNeedle = ''-kext "$KEXTS/PDArmPlatformExpert.kext"'';
  corecrypto = import ./build-corecrypto.nix;
  corecryptoNeedle = ''-kext "$KEXTS/corecrypto.kext"'';
  swap = needle: replacement: text:
    let out = builtins.replaceStrings [ needle ] [ replacement ] text;
    in assert needle == replacement || out != text; out;
in base.overrideAttrs (old: {
  buildPhase = swap "${baseKernel}" "${kernel}"
    (swap platformNeedle
      ''-kext "${platform}/System/Library/Extensions/PDArmPlatformExpert.kext"''
      (swap corecryptoNeedle
        ''-kext "${corecrypto}/System/Library/Extensions/corecrypto.kext"''
        old.buildPhase));
})
