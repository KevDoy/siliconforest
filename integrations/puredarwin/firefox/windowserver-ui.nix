# SPDX-License-Identifier: MIT
# Private GPU presenter plus finger pixel/phase input; no installed replacement.
let
 a=import ../cocoa/arm64.nix;
 p=import ../cocoa/patched.nix;
 src=a.pkgs.runCommand "sffx-windowserver-ui-source" {} ''
  cp -r ${p.windowserverSrc} $out
  chmod -R u+w $out
  cp ${./gpu-present.h} $out/sffx_gpu_present.h
  cd $out
  ${a.pkgs.patch}/bin/patch --batch -p1 < ${./windowserver-patches/0001-opt-in-whole-window-gpu-layer-presentation.patch}
  ${a.pkgs.patch}/bin/patch --batch -p1 < ${./windowserver-patches/0002-opt-in-finger-pixels-and-scroll-phase.patch}
 '';
in p.windowserver.override {inherit src;}
