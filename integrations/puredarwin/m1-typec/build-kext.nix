# SPDX-License-Identifier: MIT
let
  source = import ../rebase/pd.nix;
  inherit (source) pd pkgs;
  copied = pkgs.runCommand "puredarwin-typec-source" {} ''
    mkdir -p "$out"
    cp ${./kernel}/* "$out/"
    # Existing, hardware-used Linux PA Semi port; no second implementation.
    cp ${../m1-sound/kernel/pd_i2c.cpp} "$out/pd_i2c.cpp"
    cp ${../m1-sound/kernel/pd_i2c.h} "$out/pd_i2c.h"
    cp ${../m1-sound/kernel/pd_soc.cpp} "$out/pd_soc.cpp"
    cp ${../m1-sound/kernel/pd_soc.h} "$out/pd_soc.h"
    cp ${./NOTICE} "$out/NOTICE"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-typec";
  src = pkgs.runCommand "puredarwin-typec-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDAppleTypeC"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDAppleTypeC"
    printf '\nadd_subdirectory(PDAppleTypeC)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDAppleTypeC.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDAppleTypeC/PDAppleTypeC.kext
    test -f "$bundle/Contents/MacOS/PDAppleTypeC"
    mkdir -p "$out/System/Library/Extensions" "$out/nix-support"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    cp ${./NOTICE} "$out/System/Library/Extensions/PDAppleTypeC.kext/Contents/NOTICE"
    runHook postInstall
  '';
})
