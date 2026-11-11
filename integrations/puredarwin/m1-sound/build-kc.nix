# SPDX-License-Identifier: MIT
# The default desktop collection (../m1-cpufreq/build-kc.nix: eight cores, PDCpuPower) plus
# PDAppleSound.kext (./build-kext.nix), with its debug map stripped like the other kexts
# that recipe adds (the collection has little room below the kernel's __TEXT).
# pdsound=0 in the boot-args leaves the audio hardware untouched.
#   nix-build ... build-kc.nix --argstr kernelConfig development
{ kernelConfig ? "development" }:
let
  pins = import ../m1-rust-kext/pins.nix;
  pkgs = pins.pkgs;
  base = import ../m1-cpufreq/build-kc.nix { inherit kernelConfig; };
  strip = "${pins.crossToolchain}/bin/arm64-apple-darwin20.4-strip";
  full = import ./build-kext.nix;
  kext = pkgs.runCommand "${full.name}-no-debug-map" { } ''
    mkdir -p $out
    cp -R ${full}/System/Library/Extensions/PDAppleSound.kext $out/bundle.kext
    chmod -R u+w $out
    ${strip} -S $out/bundle.kext/Contents/MacOS/PDAppleSound
  '';
  needle = "-o kernel";
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = builtins.replaceStrings [ needle ] [ ''-kext "${kext}/bundle.kext" \
      -o kernel'' ] old.buildPhase;
  })
