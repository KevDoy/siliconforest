# SPDX-License-Identifier: MIT
let
  source = import ../rebase/pd.nix;
  inherit (source) pd pkgs;
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-dcpext-plan-objects";
  src = pkgs.runCommand "puredarwin-dcpext-plan-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDDcpExtPlan"
    mkdir -p "$dest"
    cp ${./include}/* ${./src}/*.c "$dest/"
    cat > "$dest/CMakeLists.txt" <<'CMAKE'
add_library(PDDcpExtPlan OBJECT pd_dcpext_plan.c)
target_include_directories(PDDcpExtPlan PRIVATE "''${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(PDDcpExtPlan PRIVATE xnu_kernel_private_headers)
target_compile_options(PDDcpExtPlan PRIVATE -Os -Wall)
CMAKE
    printf '\nadd_subdirectory(PDDcpExtPlan)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDDcpExtPlan
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    cp build-nix/src/Kernel/Extensions/PDDcpExtPlan/CMakeFiles/PDDcpExtPlan.dir/*.o "$out/objects/"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
