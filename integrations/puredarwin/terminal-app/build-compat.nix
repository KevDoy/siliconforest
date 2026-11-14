# SPDX-License-Identifier: MIT
# The macOS-application compatibility libraries of ./compat (GitHub issue 52,
# docs/roadmap/issues/SF-Y-terminal-app.md), built by ./compat/build.sh against the libraries a
# SiliconForest root has, each from its own recipe (no copy of a running system's root):
#
#   wrappers at a framework's path, which add what the framework lacks and re-export it:
#     CoreFoundation, Foundation (at those two paths the root has links to /usr/lib/lib*.dylib;
#       the wrappers re-export the libraries, Foundation's also the Swift overlay: upstream's
#       framework-compat layout), QuartzCore, Carbon, CoreServices, AudioToolbox (the real
#       library stays beside its wrapper as <name>.pd: this recipe puts both)
#   new: ColorSync, DataDetectorsCore, CoreAnalytics (private), libScreenReader
#
# A tree rooted at /. What the wrapped libraries are built from:
#   libSystem, libFoundation, the Swift runtime   ./build-libsystem.nix, ./build-foundation.nix, ./build-swift-runtime.nix
#   libobjc, libCoreFoundation, libicucore        upstream at ../rebase/pd.nix (the root's own)
#   QuartzCore, Carbon                            ../macos-compat/build-appkit.nix (the Cocoa stream's root)
#   CoreServices                                  ../macos-compat/build-coreservices.nix
#   AudioToolbox                                  ../m1-sound/sysaudio/build-sysaudio.nix
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-compat.nix
let
  pd = (import ../rebase/pd.nix).pd;
  p = pd.packages.x86_64-linux;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  libsystem = import ./build-libsystem.nix;
  foundation = import ./build-foundation.nix;
  swift = import ./build-swift-runtime.nix;
  appkitRoot = import ../macos-compat/build-appkit.nix;
  coreservices = import ../macos-compat/build-coreservices.nix;
  audiotoolbox = (import ../m1-sound/sysaudio/build-sysaudio.nix).audiotoolbox;
  sdk = "${p.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  F = "System/Library/Frameworks";
in pkgs.runCommand "siliconforest-macos-app-compat-arm64" { nativeBuildInputs = [ pkgs.python3 pkgs.bash ]; } ''
  set -o pipefail
  # the link root: the real libraries at their install paths
  L=$PWD/linkroot; mkdir -p $L/usr/lib/swift $L/${F}
  cp -L ${libsystem}/usr/lib/libSystem.B.dylib $L/usr/lib/
  cp -a ${libsystem}/usr/lib/system $L/usr/lib/ 2>/dev/null || true
  cp -L ${foundation}/usr/lib/libFoundation.dylib ${p.libobjc-arm64}/usr/lib/libobjc.A.dylib ${p.coreFoundation-arm64}/usr/lib/libCoreFoundation.dylib $L/usr/lib/
  ln -s libobjc.A.dylib $L/usr/lib/libobjc.dylib
  for f in ${p.icuCore-arm64}/usr/lib/libicu*.dylib; do cp -P $f $L/usr/lib/; done
  cp -L ${swift}/usr/lib/swift/*.dylib $L/usr/lib/swift/
  # (Carbon is an umbrella: it re-exports frameworks inside it)
  for n in QuartzCore Carbon; do cp -a ${appkitRoot}/${F}/$n.framework $L/${F}/; done
  mkdir -p $L/${F}/CoreServices.framework/Versions/A $L/${F}/AudioToolbox.framework/Versions/A
  cp -L ${coreservices}/${F}/CoreServices.framework/Versions/A/CoreServices $L/${F}/CoreServices.framework/Versions/A/
  cp -L ${audiotoolbox}/${F}/AudioToolbox.framework/Versions/A/AudioToolbox $L/${F}/AudioToolbox.framework/Versions/A/
  chmod -R u+w $L
  mkdir -p src/compat src/hb; cp ${./compat}/* src/compat/; cp ${../homebrew/hb_macho.py} src/hb/hb_macho.py
  export CC=${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-clang LD=${p.native-ld}/bin/ld SDK=${sdk} LINKROOT=$L PY=python3 HB=$PWD/src/hb
  export INC="-I${libsystem}/usr/include -I${p.libobjc-arm64}/usr/include -I${p.coreFoundation-arm64}/include -I${foundation}/usr/include -I${p.icuCore-arm64}/include"
  export HOME=$PWD
  bash src/compat/build.sh $out > build.log 2>&1 || { tail -40 build.log; exit 1; }
  # the wrapped libraries beside their wrappers
  for n in QuartzCore Carbon CoreServices AudioToolbox; do cp $L/${F}/$n.framework/Versions/A/$n $out/${F}/$n.framework/Versions/A/$n.pd; done
  find $out -type f -exec chmod 755 {} +
  mkdir -p $out/nix-support; cp build.log $out/nix-support/build.log
''
