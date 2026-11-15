# SPDX-License-Identifier: MIT
# SDK compilation of the directly ported PHY programs; no attachment or MMIO.
let
  source = import ../rebase/pd.nix;
  inherit (source) pd pkgs;
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-atc-objects";
  src = pkgs.runCommand "puredarwin-atc-objects-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDATCLogic"
    mkdir -p "$dest"
    cp ${./include}/* ${./src}/*.c ${./upstream}/*.inc "$dest/"
    cat > "$dest/CMakeLists.txt" <<'CMAKE'
add_library(PDATCLogic OBJECT pd_atc_usb2.c pd_atc_dp.c pd_atc_modes.c pd_atc_calibration.c)
target_include_directories(PDATCLogic PRIVATE "''${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(PDATCLogic PRIVATE xnu_kernel_private_headers)
target_compile_options(PDATCLogic PRIVATE -Os -Wall)
CMAKE
    printf '\nadd_subdirectory(PDATCLogic)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDATCLogic
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    cp build-nix/src/Kernel/Extensions/PDATCLogic/CMakeFiles/PDATCLogic.dir/*.o "$out/objects/"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
