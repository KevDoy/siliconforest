# SPDX-License-Identifier: MIT
# libpdavd (static, with its headers) and pdavd-replay for the PureDarwin M1 guest (arm64
# Mach-O, libSystem only): same toolchain and link shape as ../m1-sound/build-pdplay.nix.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-user.nix
let
  pd = (import ../rebase/pd.nix).pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  p = pd.packages.x86_64-linux;
  libSystem = import ../m1-userspace-diagnostics/build-libsystem-spawnattr.nix;
in pkgs.stdenv.mkDerivation {
  name = "darwin-lab-pdavd-user-arm64";
  dontUnpack = true;
  buildPhase = ''
    export DARWIN_SDK_ROOT=${p.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    CC="${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-clang -std=c11 -Wall -Wextra -O2
      -isystem $DARWIN_SDK_ROOT/usr/include -I${./include} -I${./user}"
    $CC -c ${./user/pdavd.c} -o pdavd.o
    ${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-ar rcs libpdavd.a pdavd.o
    $CC ${./user/pdavd-replay.c} pdavd.o \
      --ld-path=${p.native-ld}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libSystem}/usr/lib/system \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -lSystem -o pdavd-replay
  '';
  installPhase = ''
    mkdir -p "$out/usr/bin" "$out/lib" "$out/include"
    cp pdavd-replay "$out/usr/bin/"
    cp libpdavd.a "$out/lib/"
    cp -R ${./include}/pdavd "$out/include/"
  '';
  dontFixup = true;
  dontStrip = true;
}
