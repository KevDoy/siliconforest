# SPDX-License-Identifier: MIT
# Native ACIO1 firmware stage. No NHI/domain/PCI enumeration is implied.
let
  lkpi=import ../m1-rust-kext/lkpi;
  inherit (import ../m1-rust-kext/pins.nix) pkgs;
in lkpi.drmKextFinal.overrideAttrs (old: {
  pname="puredarwin-apple-cio";
  src=pkgs.runCommand "puredarwin-apple-cio-tree" {} ''
    cp -R ${lkpi.finalTree} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDAppleCIO"
    mkdir -p "$dest"
    cp ${./kernel}/* "$dest/"
    cp ${./include}/* "$dest/"
    cp ${./src}/pd_apple_nhi.c ${./src}/pd_cio_tunables.c "$dest/"
    cp ${../m1-usb-dwc3/kernel/pd_usbc_phy_lease.h} "$dest/pd_usbc_phy_lease.h"
    cp ${../m1-usb-dwc3/kernel/pd_usb_iova.h} "$dest/pd_usb_iova.h"
    cp ${../m1-dcp-dart/include/pd_dart_table.h} "$dest/pd_dart_table.h"
    cp ${../m1-dcp-dart/src/pd_dart_table.c} "$dest/pd_dart_table.c"
    cp ${../m1-sound/kernel}/pd_soc.* "$dest/"
    chmod u+w "$dest/pd_soc.cpp"
    cat ${./kernel/pd_cio_pmgr.inc} >> "$dest/pd_soc.cpp"
    cp -R ${../m1-gpu-rtkit/src} "$dest/rtkit"
    printf '\nadd_subdirectory(PDAppleCIO)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase=''
    runHook preBuild
    ninja -C build-nix -k 0 PDAppleCIO.kext
    runHook postBuild
  '';
  installPhase=''
    mkdir -p "$out/System/Library/Extensions" "$out/nix-support"
    bundle=build-nix/src/Kernel/Extensions/PDAppleCIO/PDAppleCIO.kext
    test -f "$bundle/Contents/MacOS/PDAppleCIO"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    cat ${./NOTICE} ${../m1-usb-dwc3/NOTICE} ${../m1-sound/NOTICE} > "$out/System/Library/Extensions/PDAppleCIO.kext/Contents/NOTICE"
    "$NIX_DARWIN_TOOLCHAIN_DIR/$NIX_DARWIN_HOST-nm" -u -j "$bundle/Contents/MacOS/PDAppleCIO" | sort -u > "$out/nix-support/undefined"
  '';
})
