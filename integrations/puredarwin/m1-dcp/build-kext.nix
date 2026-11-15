# Builds PDDcp.kext (and PDDRMDri.kext from the same tree): drivers/gpu/drm/apple of the pin
# with ./patches, on the layer and DRM core PDAgx uses (m1-rust-kext/lkpi: finalTree,
# finalDrmkmod), with m1-gpu-rtkit's RTKit port and m1-dcp-dart. See kext/CMakeLists.txt.
# $out/nix-support: build-commands, undefined (nm -u), PDDcp.map.
let
  inherit (import ../m1-rust-kext/pins.nix) pkgs crossToolchain;
  lkpi = import ../m1-rust-kext/lkpi;
  pin = /home/chrisk/src/darwin-lab/sources/gpu-plan/linux-aurora;
  driver = builtins.path { path = pin + "/drivers/gpu/drm/apple"; name = "linux-aurora-drm-apple"; };
  pinFile = p: builtins.path { path = pin + "/${p}"; name = builtins.baseNameOf p; };
  headers = pkgs.lib.filter (h: h != "") (pkgs.lib.splitString "\n" (builtins.readFile ./tools/pin-headers.txt));
  k = ./kext;
  staged = pkgs.runCommand "puredarwin-dcp-source" { nativeBuildInputs = [ pkgs.patch ]; } ''
    mkdir -p $out/pin $out/pininc $out/include
    cp ${k}/CMakeLists.txt ${k}/Info.plist ${k}/glue.c ${k}/pd_dcp_linux.c ${k}/pd_dcp_mmap.c ${k}/pd_dcp_prime.c ${k}/pd_dcp_drm72.c ${k}/PDDcpHost.cpp ${k}/pd_dcp_backlight.c ${k}/pd_dcp_hotplug.c ${k}/PDDcpSysctl.cpp $out/
    cp ${./include/pd_dcp_compat.h} $out/include/pd_dcp_compat.h
    cp -r ${driver} $out/driver; chmod -R u+w $out/driver
    for p in ${./patches}/*.patch; do patch -d $out/driver -p1 -F0 --no-backup-if-mismatch < $p; done
    ${pkgs.lib.concatMapStrings (f: "cp ${pinFile f} $out/pin/${builtins.baseNameOf f}\n") [
      "drivers/gpu/drm/drm_gem_dma_helper.c" "drivers/gpu/drm/drm_fb_dma_helper.c"
      "drivers/gpu/drm/drm_gem_framebuffer_helper.c" "drivers/gpu/drm/drm_simple_kms_helper.c"
      "drivers/base/component.c" ]}
    ${pkgs.lib.concatMapStrings (h: "mkdir -p $out/pininc/${builtins.dirOf h}; cp ${pinFile "include/${h}"} $out/pininc/${h}\n") headers}
    # linux/gpio/consumer.h includes a private "defs.h" only kernel builds have
    : > $out/pininc/linux/gpio/defs.h
    cp -r ${lkpi.rtkitSrc}/. $out/rtkit
    mkdir -p $out/dart; cp -r ${../m1-dcp-dart/include} $out/dart/include; cp -r ${../m1-dcp-dart/src} $out/dart/src
  '';
in lkpi.drmKextFinal.overrideAttrs (old: {
  pname = "puredarwin-dcp";
  src = pkgs.runCommand "puredarwin-dcp-tree" {} ''
    cp -R ${lkpi.finalTree} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDDcp"
    cp -R ${staged} "$out/src/Kernel/Extensions/PDDcp"
    printf '\nadd_subdirectory(PDDcp)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix -t commands PDDcp.kext > build-commands.txt
    ninja -C build-nix -k 0 PDDcp.kext PDDRMDri.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDDcp/PDDcp.kext
    test -f "$bundle/Contents/MacOS/PDDcp"
    mkdir -p "$out/System/Library/Extensions" "$out/nix-support"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    cp -R build-nix/src/Kernel/Extensions/PDDRMDri/PDDRMDri.kext "$out/System/Library/Extensions/"
    cp build-commands.txt "$out/nix-support/build-commands"
    cp build-nix/PDDcp.map "$out/nix-support/PDDcp.map"
    ${crossToolchain}/bin/arm64-apple-darwin20.4-nm -u "$bundle/Contents/MacOS/PDDcp" | sed 's/^ *//' | sort -u > "$out/nix-support/undefined"
    # the kext link accepts undefined symbols (the kernel and other kexts provide them at
    # collection time), so a layer function that nothing defines (a missing extern "C", a
    # misspelt name) links and then faults at its first call. Names of this layer must be
    # defined here; the three families below come from PDDRMDri and the kernel (pd_usb_*
    # is referenced by the USB bus glue, which no driver of this machine registers with).
    bad=$(grep -E '^_(pd|linux_|lkpi_)' "$out/nix-support/undefined" | grep -Ev '^_pd_(dri|lfile|usb)_' || true)
    if [ -n "$bad" ]; then
      echo "PDDcp: layer symbols that nothing defines:" >&2; echo "$bad" >&2
      exit 1
    fi
    runHook postInstall
  '';
})
