# SPDX-License-Identifier: MIT
# Native ACIO1 firmware stage. No NHI/domain/PCI enumeration is implied.
{ connectionManager ? false, pciEnumeration ? false, endpointMapper ? false, pciMSI ? false }:
assert !pciEnumeration || connectionManager;
assert !endpointMapper || pciEnumeration;
assert !pciMSI || (pciEnumeration && endpointMapper);
let
  lkpi=import ../m1-rust-kext/lkpi;
  inherit (import ../m1-rust-kext/pins.nix) pkgs;
in lkpi.drmKextFinal.overrideAttrs (old: {
  pname="puredarwin-apple-cio";
  src=pkgs.runCommand "puredarwin-apple-cio-tree" { nativeBuildInputs=[ pkgs.python3 ]; } ''
    cp -R ${lkpi.finalTree} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDAppleCIO"
    mkdir -p "$dest"
    cp ${./kernel}/* "$dest/"
    ${pkgs.lib.optionalString endpointMapper ''
      chmod u+w "$dest/CMakeLists.txt"
      substituteInPlace "$dest/CMakeLists.txt" --replace-fail \
        '"''${CMAKE_CURRENT_SOURCE_DIR}/PDCIOPCIController.cpp"' \
        '"''${CMAKE_CURRENT_SOURCE_DIR}/PDCIOPCIController.cpp" "''${CMAKE_CURRENT_SOURCE_DIR}/PDCIOPCIDartMapper.cpp"'
    ''}
    ${pkgs.lib.optionalString pciEnumeration ''
      chmod u+w "$dest/CMakeLists.txt"
      printf '\ntarget_sources(PDAppleCIO.kext PRIVATE PDCIOPCIEnumeration.cpp)\ntarget_compile_definitions(PDAppleCIO.kext PRIVATE PD_CIO_PCIE_ENUMERATION=1)\n' >> "$dest/CMakeLists.txt"
    ''}
    ${pkgs.lib.optionalString pciMSI ''
      printf '\ntarget_sources(PDAppleCIO.kext PRIVATE PDCIOPCIMSIController.cpp)\ntarget_compile_definitions(PDAppleCIO.kext PRIVATE PD_CIO_PCI_MSI=1)\nset_source_files_properties(PDCIOPCIMSIController.cpp PDCIOPCIHost.cpp PROPERTIES COMPILE_DEFINITIONS KERNEL_PRIVATE=1)\n' >> "$dest/CMakeLists.txt"
    ''}
    cp ${./include}/* "$dest/"
    cp ${./src}/pd_apple_nhi.c ${./src}/pd_cio_tunables.c ${./src}/pd_pciec_config.c ${./src}/pd_pciec_port.c ${./src}/pd_pciec_tunables.c "$dest/"
    cp ${../m1-usb-dwc3/kernel/pd_usbc_phy_lease.h} "$dest/pd_usbc_phy_lease.h"
    cp ${../m1-usb-dwc3/kernel/pd_usb_iova.h} "$dest/pd_usb_iova.h"
    cp ${../m1-dcp-dart/include/pd_dart_table.h} "$dest/pd_dart_table.h"
    cp ${../m1-dcp-dart/src/pd_dart_table.c} "$dest/pd_dart_table.c"
    cp ${../m1-sound/kernel}/pd_soc.* "$dest/"
    chmod u+w "$dest/pd_soc.cpp"
    cat ${./kernel/pd_cio_pmgr.inc} >> "$dest/pd_soc.cpp"
    cp -R ${../m1-gpu-rtkit/src} "$dest/rtkit"
    chmod -R u+w "$dest/rtkit"
    # Keep ACK contents, ordering and return values; count their actual
    # results in this controller's own engine. Shared RTKit sources stay put.
    python3 ${./tools/observe_rtkit.py} "$dest/rtkit/rtkit.c"
    # This controller owns 24 NHI lines + DART + its two mailbox lines.
    # Keep all other RTKit consumers' private engines unchanged.
    substituteInPlace "$dest/rtkit/PDRtkitPlatform.cpp" --replace-fail '#define MAX_IRQS  16' '#define MAX_IRQS  64'
    substituteInPlace "$dest/rtkit/PDRtkitPlatform.cpp" --replace-fail 'q->handled++;' '__atomic_add_fetch(&q->handled,1,__ATOMIC_RELAXED);'
    cat ${./kernel/pd_cio_irq.inc} >> "$dest/rtkit/PDRtkitPlatform.cpp"
    ${pkgs.lib.optionalString connectionManager ''
      cm="$out/src/Kernel/Extensions/PDThunderboltCM"
      mkdir -p "$cm"
      cp -R ${./upstream/family} "$cm/driver"
      cp -R ${./upstream/lib} "$cm/lib"
      cp -R ${./pininc} "$cm/pininc"
      chmod -R u+w "$cm/pininc"
      cp -R ${./nativeinc}/. "$cm/pininc/"
      cp ${./native}/*.h ${./native}/*.c "$cm/"
      cp ${./include/pd_tb_registry.h} "$cm/pd_tb_registry.h"
      cp ${./include/pd_tb_wire.h} "$cm/pd_tb_wire.h"
      cp ${./src/pd_tb_registry.c} "$cm/pd_tb_registry.c"
      cp ${./pd_tb_cm_compat.h} "$cm/pd_tb_cm_compat.h"
      python3 ${./native/patch_device.py} "$out"
      # Preserve every BSD initializer/linker-set member explicitly while
      # dropping unreachable CM/compatibility functions from this larger kext.
      sets="$out/src/Kernel/fbsdkpi/haiku/freebsd_network/compat/sys/linker_set.h"
      chmod u+w "$sets"
      substituteInPlace "$sets" --replace-fail '__section("__DATA," __PDSET_##set) __used' '__section("__DATA," __PDSET_##set ",regular,no_dead_strip") __used'

      chmod u+w "$out/src/Kernel/fbsdkpi/freebsd/linuxkpi/common/src/linux_devres.c"
      cat ${./native/pd_tb_devres_groups.inc} >> "$out/src/Kernel/fbsdkpi/freebsd/linuxkpi/common/src/linux_devres.c"
      chmod -R u+w "$cm"
      python3 ${./tools/prepare_cm.py} "$cm/driver"
      cp ${./cm-CMakeLists.txt} "$cm/CMakeLists.txt"
      cp ${./native}/*.h "$dest/"
      cp ${../m1-typec/kernel/pd_cd321x.h} "$dest/pd_cd321x.h"
    ''}
    printf '\nadd_subdirectory(PDAppleCIO)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase=''
    runHook preBuild
    ninja -C build-nix -k 0 PDAppleCIO.kext
    runHook postBuild
  '';
  installPhase=''
    mkdir -p "$out/System/Library/Extensions" "$out/nix-support"
    bundle=build-nix/src/Kernel/Extensions/PDAppleCIO/PDAppleCIO.kext
    test -f "$bundle/Contents/MacOS/PDAppleCIO"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    cat ${./NOTICE} ${../m1-usb-dwc3/NOTICE} ${../m1-sound/NOTICE} > "$out/System/Library/Extensions/PDAppleCIO.kext/Contents/NOTICE"
    "$NIX_DARWIN_TOOLCHAIN_DIR/$NIX_DARWIN_HOST-nm" -u -j "$bundle/Contents/MacOS/PDAppleCIO" | sort -u > "$out/nix-support/undefined"
  '';
})
