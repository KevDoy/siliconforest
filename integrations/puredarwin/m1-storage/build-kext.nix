# SPDX-License-Identifier: MIT
# Builds PDAppleNVMe.kext (GPL-2.0: READ-ONLY driver for the internal NVMe storage of T8103) in the
# pinned PureDarwin tree. Sources: ./kernel.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-apple-nvme-source" {} ''
    mkdir -p "$out"
    cp ${./kernel}/* "$out/"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-apple-nvme";
  src = pkgs.runCommand "puredarwin-apple-nvme-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDAppleNVMe"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDAppleNVMe"
    printf '\nadd_subdirectory(PDAppleNVMe)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDAppleNVMe.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDAppleNVMe/PDAppleNVMe.kext
    test -f "$bundle/Contents/MacOS/PDAppleNVMe"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
