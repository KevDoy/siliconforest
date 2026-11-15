# SPDX-License-Identifier: MIT
# SDK compilation only; no USB host service or collection member yet.
let
  source = import ../rebase/pd.nix;
  inherit (source) pd pkgs;
  copied = pkgs.runCommand "puredarwin-usb-dma-source" {} ''
    mkdir -p "$out"
    cp ${./kernel}/* "$out/"
    cp ${../m1-dcp-dart/include/pd_dart_table.h} "$out/pd_dart_table.h"
    cp ${../m1-dcp-dart/src/pd_dart_table.c} "$out/pd_dart_table.c"
    cp ${../m1-sound/kernel/pd_soc.cpp} "$out/pd_soc.cpp"
    cp ${../m1-sound/kernel/pd_soc.h} "$out/pd_soc.h"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-usb-dma-objects";
  src = pkgs.runCommand "puredarwin-usb-dma-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDUSBDma"
    printf '\nadd_subdirectory(PDUSBDma)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDUSBDma
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    cp build-nix/src/Kernel/Extensions/PDUSBDma/CMakeFiles/PDUSBDma.dir/*.o "$out/objects/"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
