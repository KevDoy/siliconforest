# SPDX-License-Identifier: MIT
# Builds PDAopAudio.kext (GPL-2.0: J293 built-in microphone capture through the AOP) in the
# pinned PureDarwin tree. Sources: ./kernel.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-aop-audio-source" {} ''
    mkdir -p "$out"
    cp ${./kernel}/* "$out/"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-aop-audio";
  src = pkgs.runCommand "puredarwin-aop-audio-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDAopAudio"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDAopAudio"
    printf '\nadd_subdirectory(PDAopAudio)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDAopAudio.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDAopAudio/PDAopAudio.kext
    test -f "$bundle/Contents/MacOS/PDAopAudio"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
