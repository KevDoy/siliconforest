# Builds PDRustHello.kext (C++ IOKit shell + Rust object) in the pinned PureDarwin tree,
# the same way ../m1-apcie-host/build-kext.nix builds its kext.
# $out/nix-support/build-commands has the exact compile and link lines ninja ran.
let
  inherit (import ./pins.nix) pd pkgs;
  rustObject = (import ./build-rust.nix).object;
  copied = pkgs.runCommand "puredarwin-rust-hello-source" {} ''
    mkdir -p "$out/rust"
    cp ${./kext/CMakeLists.txt} "$out/CMakeLists.txt"
    cp ${./kext/Info.plist} "$out/Info.plist"
    cp ${./kext/PDRustHello.cpp} "$out/PDRustHello.cpp"
    cp ${./kext/pd_rust_hello.h} "$out/pd_rust_hello.h"
    cp ${rustObject}/pdrusthello.o "$out/rust/pdrusthello.o"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-rust-hello";
  src = pkgs.runCommand "puredarwin-rust-hello-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDRustHello"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDRustHello"
    printf '\nadd_subdirectory(PDRustHello)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix -t commands PDRustHello.kext > build-commands.txt
    ninja -C build-nix PDRustHello.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDRustHello/PDRustHello.kext
    test -f "$bundle/Contents/MacOS/PDRustHello"
    mkdir -p "$out/System/Library/Extensions" "$out/nix-support"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    cp build-commands.txt "$out/nix-support/build-commands"
    runHook postInstall
  '';
})
