# Reuse the exact baseline's kext configure/toolchain. Only the platform bundle
# is built and installed; source patching is a separate immutable derivation.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
in
pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-m1-bringup-platform";
  src = pkgs.applyPatches {
    name = "puredarwin-m1-platform-source";
    src = old.src;
    patches = [ ./platform.patch ];
  };
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDArmPlatformExpert
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDArmPlatformExpert/PDArmPlatformExpert.kext
    test -f "$bundle/Contents/MacOS/PDArmPlatformExpert"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
