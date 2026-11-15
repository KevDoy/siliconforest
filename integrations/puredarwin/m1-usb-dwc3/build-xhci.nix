# SPDX-License-Identifier: MIT
# Experimental replacement of RavynXHCIPort; not part of the daily collection.
let
  source = import ../rebase/pd.nix;
  inherit (source) pd pkgs;
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-apple-xhci";
  src = pkgs.runCommand "puredarwin-apple-xhci-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    cd "$out"
    patch -p1 < ${./patches/apple-dma-lifetime.patch}
    dest="$out/src/Kernel/Extensions/RavynXHCIPort"
    cp ${./kernel}/pd_usb_* "$dest/"
    cp ${../m1-dcp-dart/include/pd_dart_table.h} "$dest/pd_dart_table.h"
    cp ${../m1-dcp-dart/src/pd_dart_table.c} "$dest/pd_dart_table.c"
    cp ${../m1-sound/kernel}/pd_soc.* "$dest/"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix RavynXHCIPort.kext
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/System/Library/Extensions"
    cp -R build-nix/src/Kernel/Extensions/RavynXHCIPort/RavynXHCIPort.kext "$out/System/Library/Extensions/"
    cp ${./NOTICE} "$out/System/Library/Extensions/RavynXHCIPort.kext/Contents/NOTICE"
  '';
})
