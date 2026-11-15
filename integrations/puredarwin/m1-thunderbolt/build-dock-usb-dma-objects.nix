# SPDX-License-Identifier: MIT
# Detached native DMA helper only. No controller binding or boot profile.
let
 source=import ../rebase/pd.nix;
 inherit (source) pkgs;
 base=import ./build-dock-usb-readonly.nix;
in base.overrideAttrs (old: {
 pname="puredarwin-dock-xhci-dma-objects";
 src=pkgs.runCommand "puredarwin-dock-xhci-dma-object-tree" {} ''
  cp -R ${old.src} "$out"
  chmod -R u+w "$out"
  dest="$out/src/Kernel/Extensions/RavynXHCIPort"
  cp ${./dock-usb}/pd_pci_usb_dma.* "$dest/"
  printf '\ntarget_sources(RavynXHCIPort.kext PRIVATE pd_pci_usb_dma.cpp)\n' >> "$dest/CMakeLists.txt"
 '';
 installPhase=''
  mkdir -p "$out/objects"
  cp build-nix/src/Kernel/Extensions/RavynXHCIPort/CMakeFiles/RavynXHCIPort.kext.dir/pd_pci_usb_dma.cpp.o "$out/objects/"
 '';
})
