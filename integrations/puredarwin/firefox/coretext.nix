# SPDX-License-Identifier: MIT
# Private CoreText candidate. Never overwrites an installed framework.
let
 a=import ../cocoa/arm64.nix;
 p=import ../cocoa/patched.nix;
 src=a.pkgs.runCommand "sffx-coretext-font-copy-source" {} ''
  mkdir -p $out/src/Frameworks
  cp -r ${p.coretextSrc} $out/src/Frameworks/CoreText
  chmod -R u+w $out
  cd $out
  ${a.pkgs.patch}/bin/patch --batch -p1 < ${./cocoa-patches/0215-coretext-copy-keeps-graphics-font-for-rendering-attributes.patch}
 '';
in p.coretext.override {src="${src}/src/Frameworks/CoreText";}
