# SPDX-License-Identifier: MIT
# Builds PDAppleTouchBar.kext (GPL-2.0: the J293 Touch Bar as a key strip) in the pinned PureDarwin
# tree. Sources: ./kernel and ./spihid/pd_touchbar_kbd.h (shared with the keyboard's driver).
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-touchbar-source" {} ''
    mkdir -p "$out"
    cp ${./kernel}/* "$out/"
    cp ${./spihid/pd_touchbar_kbd.h} "$out/pd_touchbar_kbd.h"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-touchbar";
  src = pkgs.runCommand "puredarwin-touchbar-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDAppleTouchBar"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDAppleTouchBar"
    printf '\nadd_subdirectory(PDAppleTouchBar)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDAppleTouchBar.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDAppleTouchBar/PDAppleTouchBar.kext
    test -f "$bundle/Contents/MacOS/PDAppleTouchBar"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
