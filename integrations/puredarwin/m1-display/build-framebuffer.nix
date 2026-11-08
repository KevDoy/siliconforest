# Compile only the firmware-framebuffer bundle with the exact pinned SDK recipe.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
in
pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-m1-display-framebuffer";
  src = pkgs.applyPatches {
    name = "puredarwin-m1-display-source";
    src = old.src;
    patches = [ ./framebuffer-30bit.patch ];
  };
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix IOGOPFramebuffer.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/IOGOPFramebuffer/IOGOPFramebuffer.kext
    test -f "$bundle/Contents/MacOS/IOGOPFramebuffer"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
