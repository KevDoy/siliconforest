# SPDX-License-Identifier: MIT
# The proposed desktop collection with sound (../m1-sound/build-kc-desktop.nix: quiet-boot
# kernel, eight cores, Wi-Fi, GPU, display, sound) plus PDAppleTouchBar.kext
# (./build-kext.nix) with its debug map stripped, as that recipe does for PDAppleSound.
# pdtouchbar=0 in the boot-args leaves the Touch Bar's hardware untouched. No default
# collection is changed by this file.
#   nix-build ... build-kc.nix --argstr kernelConfig development --no-out-link
{ kernelConfig ? "development" }:
let
  pins = import ../m1-rust-kext/pins.nix;
  pkgs = pins.pkgs;
  base = import ../m1-sound/build-kc-desktop.nix { inherit kernelConfig; };
  strip = "${pins.crossToolchain}/bin/arm64-apple-darwin20.4-strip";
  full = import ./build-kext.nix;
  kext = pkgs.runCommand "${full.name}-no-debug-map" { } ''
    mkdir -p $out
    cp -R ${full}/System/Library/Extensions/PDAppleTouchBar.kext $out/bundle.kext
    chmod -R u+w $out
    ${strip} -S $out/bundle.kext/Contents/MacOS/PDAppleTouchBar
  '';
  needle = "-o kernel";
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = builtins.replaceStrings [ needle ] [ ''-kext "${kext}/bundle.kext" \
      -o kernel'' ] old.buildPhase;
  })
