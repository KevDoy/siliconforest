# SPDX-License-Identifier: MIT
# Private memory-font correction; canonical CoreText adoption belongs to TERM.
let
 a=import ../cocoa/arm64.nix;
 p=import ../cocoa/patched.nix;
 src=a.pkgs.runCommand "sffx-coretext-memory-font-source" {} ''
  mkdir -p $out/src/Frameworks
  cp -r ${p.coretextSrc} $out/src/Frameworks/CoreText
  chmod -R u+w $out
  cd $out
  ${a.pkgs.patch}/bin/patch --batch --forward -p1 < ${./cocoa-patches/0220-coretext-memory-font-descriptors-without-filesystem.patch}
 '';
in p.coretext.override {src="${src}/src/Frameworks/CoreText";}
