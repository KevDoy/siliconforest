# Builds PDRustPlat.kext: a Rust platform::Driver bound to the GPU node PDGpuDT publishes.
# The kext is built in the flake's arm64 DRM-kext tree slice (drm-msm-arm64: fbsdkpi, the
# drm-kmod source, PD_DRMKMOD_DIR) with lkpi/patches-layer applied (lkpi.tree), and
# lkpi/pdrustplat/kext added as PDRustPlat.
# $out/nix-support/build-commands has the compile and link lines ninja ran; the build fails
# if the defines LinuxKPI was compiled with differ from the ones bindgen saw (lkpi/cflags.nix).
let
  inherit (import ./pins.nix) pd pkgs;
  lkpi = import ./lkpi;
  k = ./lkpi/pdrustplat/kext;
  copied = pkgs.runCommand "puredarwin-rust-plat-source" {} ''
    mkdir -p "$out/rust"
    cp ${k}/CMakeLists.txt ${k}/Info.plist ${k}/glue.c "$out/"
    cp ${lkpi.platObject}/pdrustplat.o "$out/rust/pdrustplat.o"
  '';
  bindgenDefines = pkgs.writeText "bindgen-defines"
    (pkgs.lib.concatMapStrings (d: "-D${d}\n") lkpi.cflags.defines);
in lkpi.drmKextFinal.overrideAttrs (old: {
  pname = "puredarwin-rust-plat";
  src = pkgs.runCommand "puredarwin-rust-plat-tree" {} ''
    cp -R ${lkpi.finalTree} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDRustPlat"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDRustPlat"
    printf '\nadd_subdirectory(PDRustPlat)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix -t commands PDRustPlat.kext > build-commands.txt

    # the layout check: -D set of a LinuxKPI source in this kext vs what bindgen was given
    grep -m1 'linux_work\.c\.o -c' build-commands.txt | tr ' ' '\n' | grep '^-D' \
      | grep -v -e KBUILD_MODNAME -e LINUXKPI_PARAM_PREFIX | sed 's/\\"/"/g' | sort -u > defines.kext
    sort -u ${bindgenDefines} > defines.bindgen
    if ! diff defines.kext defines.bindgen; then
      echo "LinuxKPI in the kext and the Rust bindings were built with different defines" >&2
      exit 1
    fi
    echo "defines match: $(wc -l < defines.kext) -D flags"

    ninja -C build-nix PDRustPlat.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDRustPlat/PDRustPlat.kext
    test -f "$bundle/Contents/MacOS/PDRustPlat"
    mkdir -p "$out/System/Library/Extensions" "$out/nix-support"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    cp build-commands.txt "$out/nix-support/build-commands"
    runHook postInstall
  '';
})
