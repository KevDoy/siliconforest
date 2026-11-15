# SPDX-License-Identifier: MIT
# Private opt-in finger input translation; baseline presentation remains intact.
let
 a=import ../cocoa/arm64.nix;
 p=import ../cocoa/patched.nix;
 src=a.pkgs.runCommand "sffx-windowserver-precise-scroll-source" {} ''
  cp -r ${p.windowserverSrc} $out
  chmod -R u+w $out
  cd $out
  ${a.pkgs.patch}/bin/patch --batch -p1 < ${./windowserver-patches/0002-opt-in-finger-pixels-and-scroll-phase.patch}
 '';
in p.windowserver.override {inherit src;}
