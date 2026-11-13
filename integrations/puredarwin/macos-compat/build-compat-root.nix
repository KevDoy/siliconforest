# SPDX-License-Identifier: MIT
# What a SiliconForest root gets for macOS programs beyond the Homebrew overlay
# (../homebrew/build-overlay.nix: libraries in /usr/lib and the small frameworks, among them
# this directory's CoreServices): one tree rooted at /.
#
#   System/Library/Frameworks/{AppKit,Cocoa,CoreData,CoreGraphics,CoreText,CoreVideo,IOSurface,
#     Onyx2D,OpenGL,QuartzCore,Carbon}.framework, usr/lib/lib{WindowServer,GL.1,GLU.1,
#     gallium-*,LLVM}.dylib, lib/lib{drm.2,fontconfig.1,freetype.*,wayland-client.0,
#     wayland-cursor.0}.dylib and their /usr/lib links        ./build-appkit.nix
#   usr/bin/perl and Perl's tools, System/Library/Perl/5.34 (libperl.dylib in
#     darwin-thread-multi-2level/CORE), Library/Perl/5.34      ./build-perl.nix
#
# Laid onto a root by ../m1-storage/tools/sf_userland_push.sh stage (the `sys` part: only paths
# the root does not have) and, for a system that runs, by tools/sf_compat_stage.sh + sf_compat_push.sh.
let
  pd = (import ../rebase/pd.nix).pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  appkit = import ./build-appkit.nix;
  perl = import ./build-perl.nix;
in pkgs.runCommand "siliconforest-compat-root-arm64" { } ''
  mkdir -p $out
  for d in ${appkit} ${perl}; do cp -a --no-preserve=mode,ownership $d/. $out/; done
  rm -rf $out/nix-support
  # a program by the same name that the base system or the userland set brings stays theirs
  rm -f $out/usr/bin/shasum
  find $out -type d -exec chmod 755 {} +
  find $out -type f \( -name '*.dylib' -o -name '*.bundle' -o -path '*/usr/bin/*' -o -path '*.framework/Versions/*/*' \) -exec chmod 755 {} + 2>/dev/null || true
''
