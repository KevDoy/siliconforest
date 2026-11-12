# SPDX-License-Identifier: MIT
# The default desktop collection (../m1-boot-speed/build-kc.nix) plus PDAppleAVD.kext
# (./build-kext.nix) with its debug map stripped. The kext does nothing unless pdavd=1 is in
# the boot arguments. No default collection is changed by this file.
#   nix-build ... build-kc.nix --no-out-link
{ kernelConfig ? "development" }:
let
  pins = import ../m1-rust-kext/pins.nix;
  pkgs = pins.pkgs;
  base = import ../m1-boot-speed/build-kc.nix { inherit kernelConfig; };
  strip = "${pins.crossToolchain}/bin/arm64-apple-darwin20.4-strip";
  full = import ./build-kext.nix;
  kext = pkgs.runCommand "${full.name}-no-debug-map" { } ''
    mkdir -p $out
    cp -R ${full}/System/Library/Extensions/PDAppleAVD.kext $out/bundle.kext
    chmod -R u+w $out
    ${strip} -S $out/bundle.kext/Contents/MacOS/PDAppleAVD
  '';
  needle = "-o kernel";
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = builtins.replaceStrings [ needle ] [ ''-kext "${kext}/bundle.kext" \
      -o kernel'' ] old.buildPhase;
  })
