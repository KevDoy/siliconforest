# SPDX-License-Identifier: MIT
# Directly ported NHI/ACIO/DP-IN code, SDK objects only; no live driver yet.
let
  source = import ../rebase/pd.nix;
  inherit (source) pd pkgs;
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-thunderbolt-objects";
  src = pkgs.runCommand "puredarwin-thunderbolt-objects-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDThunderboltLogic"
    mkdir -p "$dest"
    cp -R ${./upstream} "$dest/upstream"
    cp -R ${./portable} "$dest/portable"
    cp -R ${./include} "$dest/include"
    cp -R ${./src} "$dest/src"
    cat > "$dest/CMakeLists.txt" <<'CMAKE'
add_library(PDThunderboltLogic OBJECT upstream/apple-dpin-state.c src/pd_apple_nhi.c src/pd_cio_lifecycle.c src/pd_cio_tunables.c src/pd_tb_registry.c)
target_include_directories(PDThunderboltLogic PRIVATE "''${CMAKE_CURRENT_SOURCE_DIR}/include" "''${CMAKE_CURRENT_SOURCE_DIR}/upstream" "''${CMAKE_CURRENT_SOURCE_DIR}/portable")
target_link_libraries(PDThunderboltLogic PRIVATE xnu_kernel_private_headers)
target_compile_options(PDThunderboltLogic PRIVATE -Os -Wall)
CMAKE
    printf '\nadd_subdirectory(PDThunderboltLogic)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDThunderboltLogic
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    cp build-nix/src/Kernel/Extensions/PDThunderboltLogic/CMakeFiles/PDThunderboltLogic.dir/src/*.o "$out/objects/"
    cp build-nix/src/Kernel/Extensions/PDThunderboltLogic/CMakeFiles/PDThunderboltLogic.dir/upstream/*.o "$out/objects/"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
