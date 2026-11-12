# SPDX-License-Identifier: MIT
# Builds PDAppleAVD.kext (GPL-2.0: the Apple Video Decoder of T8103 as /dev/pdavd) in the pinned
# PureDarwin tree. Sources: ./kernel (with the unchanged Linux files in ./kernel/linux) and
# ./include/pdavd (the ioctl interface and Linux's v4l2-controls.h).
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = (import ../rebase/pd.nix).pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-apple-avd-source" {} ''
    mkdir -p "$out"
    cp -R ${./kernel} "$out/kernel"
    cp -R ${./include} "$out/include"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-apple-avd";
  src = pkgs.runCommand "puredarwin-apple-avd-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDAppleAVD"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDAppleAVD"
    chmod -R u+w "$out/src/Kernel/Extensions/PDAppleAVD"
    printf '\nadd_subdirectory(PDAppleAVD/kernel)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDAppleAVD.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDAppleAVD/kernel/PDAppleAVD.kext
    test -f "$bundle/Contents/MacOS/PDAppleAVD"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
