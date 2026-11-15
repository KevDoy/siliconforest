# SPDX-License-Identifier: MIT
# Compile the actual pinned Linux software CM/control-channel against LinuxKPI.
# Experimental object target: not a kext, native NHI service or hardware test.
{ full ? true }:
let
  lkpi = import ../m1-rust-kext/lkpi;
  inherit (import ../m1-rust-kext/pins.nix) pkgs;
in lkpi.drmKextFinal.overrideAttrs (old: {
  pname = "puredarwin-thunderbolt-cm-objects";
  src = pkgs.runCommand "puredarwin-thunderbolt-cm-tree" { nativeBuildInputs = [ pkgs.python3 ]; } ''
    cp -R ${lkpi.finalTree} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDThunderboltCM"
    mkdir -p "$dest"
    cp -R ${./upstream/family} "$dest/driver"
    cp -R ${./upstream/lib} "$dest/lib"
    cp -R ${./pininc} "$dest/pininc"
    chmod -R u+w "$dest/pininc"
    cp -R ${./nativeinc}/. "$dest/pininc/"
    cp ${./native}/*.h ${./native}/*.c "$dest/"
    cp ${./include/pd_tb_registry.h} "$dest/pd_tb_registry.h"
    cp ${./src/pd_tb_registry.c} "$dest/pd_tb_registry.c"
    python3 ${./native/patch_device.py} "$out"
    cat ${./native/pd_tb_devres_groups.inc} >> "$out/src/Kernel/fbsdkpi/freebsd/linuxkpi/common/src/linux_devres.c"
    cp ${./pd_tb_cm_compat.h} "$dest/pd_tb_cm_compat.h"
    cp ${./include/pd_tb_wire.h} "$dest/pd_tb_wire.h"
    chmod -R u+w "$dest"
    # Tracepoints are instrumentation only; the native port has no Linux
    # tracepoint subscribers. Keep every protocol/control operation intact.
    python3 ${./tools/prepare_cm.py} "$dest/driver"
    cp ${./cm-CMakeLists.txt} "$dest/CMakeLists.txt"
    printf '\nadd_subdirectory(PDThunderboltCM)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  # The inherited hand-written configurePhase does not consume cmakeFlags.
  configurePhase = let
    phase = builtins.replaceStrings
      [ "cmake -S . -B build-nix -G Ninja" ]
      [ "cmake -S . -B build-nix -G Ninja -DPD_TB_CM_FULL=${if full then "ON" else "OFF"}" ]
      old.configurePhase;
  in assert phase != old.configurePhase; phase;
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix -k 0 PDThunderboltCM
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    find build-nix/src/Kernel/Extensions/PDThunderboltCM/CMakeFiles/PDThunderboltCM.dir -name '*.o' -exec cp {} "$out/objects/" \;
    expected=${if full then "31" else "15"}
    test "$(find "$out/objects" -name '*.o' | wc -l)" -eq "$expected"
    ${if full then "test -f \"$out/objects/domain.c.o\"" else "test ! -f \"$out/objects/domain.c.o\""}
    mkdir -p "$out/nix-support"
    nm="$NIX_DARWIN_TOOLCHAIN_DIR/$NIX_DARWIN_HOST-nm"
    "$nm" -g --defined-only -j "$out"/objects/*.o | sort -u > "$out/nix-support/defined"
    "$nm" -u -j "$out"/objects/*.o | sort -u > "$out/nix-support/undefined"
    comm -23 "$out/nix-support/undefined" "$out/nix-support/defined" > "$out/nix-support/required-external"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
