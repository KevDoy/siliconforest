# SPDX-License-Identifier: MIT
# Rebase automatically on the current Cocoa owner's assembled Onyx2D tree.
let
 a = import ../cocoa/arm64.nix;
 p = import ../cocoa/patched.nix;
 src = a.pkgs.runCommand "sffx-onyx-serialized-source" {} ''
   mkdir -p $out/src/Frameworks
   cp -r ${p.onyx2dSrc} $out/src/Frameworks/Onyx2D
   chmod -R u+w $out
   cd $out
   ${a.pkgs.patch}/bin/patch -p1 < ${./cocoa-patches/0200-onyx2d-serialize-shared-freetype-raster.patch}
 '';
in p.onyx2d.override { src = "${src}/src/Frameworks/Onyx2D"; }
