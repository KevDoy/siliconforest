# Keep PDGOP's string-based PDIOKitLib calls bound to the matching static IOKit.
# libIOKitCF has different matching-function signatures and is not interchangeable.
let
  source = import ../rebase/pd.nix;
  pd = source.pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
in pd.packages.x86_64-linux.libsystem-arm64.overrideAttrs (old: {
  pname = "puredarwin-chonkstep-pdgop-static";
  src = pkgs.applyPatches {
    name = "puredarwin-chonkstep-pdgop-source";
    src = old.src;
    # At the tip upstream's PDGOP.c has only the CoreFoundation form of the IOKit calls; the
    # first patch puts the string-based branch back (see the note at the top of this file).
    patches = source.pick {
      pin = [ ../m1-wayland/pdgop-fail-closed.patch ];
      tip = [ ../m1-wayland/pdgop-string-iokit-tip.patch ../m1-wayland/pdgop-fail-closed.patch ];
    };
  };
  postPatch = (old.postPatch or "") + ''
    # These archives are linked later against the already-built libSystem.
    # Do not rebuild syscall assembly just to create a static IOKit archive.
    substituteInPlace src/Libraries/IOKit/CMakeLists.txt \
      --replace-fail "    libsystem_kernel_static" ""
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDGOP
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    mkdir -p "$out/lib" "$out/include"
    cp build-nix/src/Libraries/PDGOP/libPDGOP.a "$out/lib/"
    cp build-nix/src/Libraries/IOKit/libIOKit.a "$out/lib/"
    cp src/Libraries/PDGOP/include/PDGOP.h "$out/include/"
    cp -R src/Libraries/IOKit/include/. "$out/include/"
    runHook postInstall
  '';
})
