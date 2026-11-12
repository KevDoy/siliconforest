# SPDX-License-Identifier: MIT
# Builds PDAppleSound.kext (GPL-2.0: J293 headphone jack playback) in the pinned PureDarwin
# tree. Sources: ./kernel and ./include.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = (import ../rebase/pd.nix).pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-sound-source" {} ''
    mkdir -p "$out"
    cp ${./kernel}/* "$out/"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-sound";
  src = pkgs.runCommand "puredarwin-sound-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDAppleSound"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDAppleSound"
    printf '\nadd_subdirectory(PDAppleSound)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDAppleSound.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDAppleSound/PDAppleSound.kext
    test -f "$bundle/Contents/MacOS/PDAppleSound"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
