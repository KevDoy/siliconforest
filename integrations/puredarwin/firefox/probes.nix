# SPDX-License-Identifier: MIT
let
  a = import ../cocoa/arm64.nix;
  inherit (a) pkgs pd;
  tc = pd.arm64-cross-toolchain;
  triple = "arm64-apple-darwin20.4";
  sdk = "${pd.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  system = pd.libsystem-arm64;
in pkgs.stdenv.mkDerivation {
  pname = "siliconforest-firefox-probes-arm64";
  version = "1";
  dontUnpack = true;
  buildPhase = ''
    ${tc}/bin/${triple}-clang -O2 -g -isysroot ${sdk} \
      -I${system}/usr/include -c ${./cgl-probe.c} -o cgl-probe.o
    ${tc}/bin/${triple}-clang -isysroot ${sdk} \
      -fuse-ld=${pd.native-ld}/bin/ld -nostdlib \
      -L${system}/usr/lib -lSystem \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${system}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 cgl-probe.o -o cgl-probe
  '';
  installPhase = ''mkdir -p $out/bin; cp cgl-probe $out/bin/'';
  dontFixup = true;
  dontStrip = true;
}
