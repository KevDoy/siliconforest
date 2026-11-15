# SPDX-License-Identifier: MIT
# Opt-in bounded actual I210 descriptor/doorbell trace. No boot profile.
let
 base=import ./build-ethernet-admission-kext.nix;
 inherit (import ../m1-rust-kext/pins.nix) pkgs;
in base.overrideAttrs (old: {
 pname="puredarwin-t8103-dock-ethernet-trace";
 src=pkgs.runCommand "puredarwin-dock-ethernet-trace-tree" {nativeBuildInputs=[pkgs.python3];} ''
  cp -R ${old.src} "$out"
  chmod -R u+w "$out"
  cp ${./include/pd_i210_trace.h} "$out/src/Kernel/Extensions/PDFBSDem/dev/e1000/pd_i210_trace.h"
  cp ${./include/pd_i210_trace.h} "$out/src/Kernel/fbsdkpi/pd/pd_i210_trace.h"
  cp ${./include/pd_i210_trace.h} "$out/src/Kernel/fbsdkpi/xnu/pd_i210_trace.h"
  cp ${./include/pd_pciec_dma_inspect.h} "$out/src/Kernel/fbsdkpi/xnu/pd_pciec_dma_inspect.h"
  python3 ${./tools/prepare_ethernet_trace.py} "$out"
  python3 ${./tools/prepare_ethernet_trace_log.py} "$out"
  python3 ${./tools/prepare_ethernet_dma_inspect.py} "$out"
 '';
})
