# Builds the GPL-2.0 PDApcieT8103Bridge.kext (host bridge revision) in the pinned PureDarwin tree.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-apcie-host-source" {} ''
    mkdir -p "$out"
    cp ${./CMakeLists.txt} "$out/CMakeLists.txt"
    cp ${./Info.plist} "$out/Info.plist"
    cp ${./PDApcieT8103.cpp} "$out/PDApcieT8103.cpp"
    cp ${./PDApcieT8103.h} "$out/PDApcieT8103.h"
    cp ${./PDApcieT8103Link.cpp} "$out/PDApcieT8103Link.cpp"
    cp ${./PDApcieT8103Link.h} "$out/PDApcieT8103Link.h"
    cp ${./PDAppleSMC.cpp} "$out/PDAppleSMC.cpp"
    cp ${./PDAppleSMC.h} "$out/PDAppleSMC.h"
    cp ${./NOTICE} "$out/NOTICE"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-apcie-host";
  src = pkgs.runCommand "puredarwin-apcie-host-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDApcieT8103Bridge"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDApcieT8103Bridge"
    printf '\nadd_subdirectory(PDApcieT8103Bridge)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDApcieT8103Bridge.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDApcieT8103Bridge/PDApcieT8103Bridge.kext
    test -f "$bundle/Contents/MacOS/PDApcieT8103Bridge"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
