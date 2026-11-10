# Builds the MIT PDGpuDT.kext (T8103 GPU device-tree synthesis) in the pinned PureDarwin tree.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-gpu-dt-source" {} ''
    mkdir -p "$out"
    cp ${./kernel/CMakeLists.txt} "$out/CMakeLists.txt"
    cp ${./kernel/Info.plist} "$out/Info.plist"
    cp ${./kernel/PDGpuDT.cpp} "$out/PDGpuDT.cpp"
    cp ${./kernel/PDGpuDT.h} "$out/PDGpuDT.h"
    cp ${./src/gpudt.c} "$out/gpudt.c"
    cp ${./src/gpudt.h} "$out/gpudt.h"
    cp ${./src/gpudt_summary.c} "$out/gpudt_summary.c"
    cp ${./src/gpudt_fw_table.inc} "$out/gpudt_fw_table.inc"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-gpu-dt";
  src = pkgs.runCommand "puredarwin-gpu-dt-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDGpuDT"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDGpuDT"
    printf '\nadd_subdirectory(PDGpuDT)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDGpuDT.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDGpuDT/PDGpuDT.kext
    test -f "$bundle/Contents/MacOS/PDGpuDT"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
