# SPDX-License-Identifier: MIT
# Private build only. Cocoa owner decides whether to merge/install proven patches.
let
 a = import ../cocoa/arm64.nix;
 p = import ../cocoa/patched.nix;
 src = a.pkgs.runCommand "sffx-appkit-headless-source" {} ''
   mkdir -p $out/src/Frameworks
   cp -r ${p.appkitSrc} $out/src/Frameworks/AppKit
   chmod -R u+w $out
   cd $out
   ${a.pkgs.patch}/bin/patch -p1 < ${./cocoa-patches/0201-appkit-headless-run-loop.patch}
   ${a.pkgs.patch}/bin/patch -p1 < ${./cocoa-patches/0203-appkit-layer-context-window-scale.patch}
   ${a.pkgs.patch}/bin/patch -p1 < ${./cocoa-patches/0204-appkit-replace-hosted-layer-root.patch}
 '';
in p.appkit.override { src = "${src}/src/Frameworks/AppKit"; }
