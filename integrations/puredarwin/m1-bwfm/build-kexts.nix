# Builds IO80211Family.kext (already in the pinned PureDarwin tree, but not in the M1 kernel
# collection) and PDBwfm.kext (injected from this directory) for arm64.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-bwfm-source" {} ''
    mkdir -p "$out"
    cp ${./CMakeLists.txt} "$out/CMakeLists.txt"
    cp ${./Info.plist} "$out/Info.plist"
    cp ${./OPENBSD_REVISION} "$out/OPENBSD_REVISION"
    cp ${./PDBwfm.cpp} "$out/PDBwfm.cpp"
    cp ${./pd_bwfm_glue.c} "$out/pd_bwfm_glue.c"
    cp ${./pd_bwfm_glue.h} "$out/pd_bwfm_glue.h"
    cp ${./pd_bwfm_compat.h} "$out/pd_bwfm_compat.h"
    cp ${../m1-apcie-host/dart/pd_dart_pool.h} "$out/pd_dart_pool.h"
    cp ${../m1-apcie-host/dart/pd_dart_pool.cpp} "$out/pd_dart_pool.cpp"
    cp ${./bwfm.c} "$out/bwfm.c"
    cp ${./bwfmreg.h} "$out/bwfmreg.h"
    cp ${./bwfmvar.h} "$out/bwfmvar.h"
    cp ${./if_bwfm_pci.c} "$out/if_bwfm_pci.c"
    cp ${./if_bwfm_pci.h} "$out/if_bwfm_pci.h"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-bwfm";
  src = pkgs.runCommand "puredarwin-bwfm-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test -d "$out/src/Kernel/Extensions/IO80211Family"
    test ! -e "$out/src/Kernel/Extensions/PDBwfm"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDBwfm"
    printf '\nadd_subdirectory(PDBwfm)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix IO80211Family.kext PDBwfm.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    mkdir -p "$out/System/Library/Extensions"
    for name in IO80211Family PDBwfm; do
      bundle=build-nix/src/Kernel/Extensions/$name/$name.kext
      test -f "$bundle/Contents/MacOS/$name"
      cp -R "$bundle" "$out/System/Library/Extensions/"
    done
    runHook postInstall
  '';
})
