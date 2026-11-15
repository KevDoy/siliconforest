# SPDX-License-Identifier: MIT
# Keep fontconfig's selected collection face through graphics-font creation.
let
 a=import ../cocoa/arm64.nix;
 p=import ../cocoa/patched.nix;
 src=a.pkgs.runCommand "sffx-onyx-collection-font-source" {} ''
  mkdir -p $out/src/Frameworks
  cp -r ${p.onyx2dSrc} $out/src/Frameworks/Onyx2D
  chmod -R u+w $out
  cd $out
  ${a.pkgs.patch}/bin/patch --batch -p1 < ${./cocoa-patches/0217-graphics-font-keeps-fontconfig-collection-face-index.patch}
 '';
in p.onyx2d.override {src="${src}/src/Frameworks/Onyx2D";}
