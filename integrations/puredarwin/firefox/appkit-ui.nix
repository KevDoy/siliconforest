# SPDX-License-Identifier: MIT
# Private matched-source UI provider. Never replaces the installed AppKit.
{ appkitSource ? null, menuGeometry ? false, preciseScroll ? false,
  gpuPresentation ? false, traceEvents ? false }:
let
 a=import ../cocoa/arm64.nix;
 p=import ../cocoa/patched.nix;
 base=if appkitSource == null then p.appkitSrc else builtins.path {
  path=appkitSource;name="sffx-selected-appkit-source";
 };
 patches=[./cocoa-patches/0212-menu-appearance-and-popup-inheritance.patch]
  ++a.pkgs.lib.optional menuGeometry ./cocoa-patches/0206-window-class-content-geometry-counts-menu-strip.patch
  ++a.pkgs.lib.optional preciseScroll ./cocoa-patches/0210-precise-scroll-deltas-and-gesture-phase.patch
  ++a.pkgs.lib.optional gpuPresentation ./cocoa-patches/0209-view-registers-gpu-layer-with-private-windowserver.patch
  ++a.pkgs.lib.optional traceEvents ./cocoa-patches/0211-diagnostic-headless-event-loop-state.patch;
 src=a.pkgs.runCommand "sffx-appkit-ui-source" {} ''
  mkdir -p $out/src/Frameworks
  cp -r ${base} $out/src/Frameworks/AppKit
  chmod -R u+w $out
  cd $out
  ${a.pkgs.lib.concatMapStringsSep "\n" (patch: ''
   ${a.pkgs.patch}/bin/patch --batch -p1 < ${patch}
  '') patches}
 '';
in (p.appkit.override {src="${src}/src/Frameworks/AppKit";}).overrideAttrs (old:{
 postInstall=(old.postInstall or "")+''
  ln -sfn A $out/System/Library/Frameworks/AppKit.framework/Versions/C
 '';
})
