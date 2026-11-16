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
  ${a.pkgs.patch}/bin/patch --batch --forward -p1 < ${./windowserver-patches/0001-opt-in-whole-window-gpu-layer-presentation.patch}
  # TERM8's main source already includes the finger pixel/phase bridge (0210w).
 '';
in p.windowserver.override {inherit src;}
