# SPDX-License-Identifier: MIT
# Guest-side diagnostics for the Homebrew stream (arm64 Mach-O against the lab's libSystem;
# same toolchain and link shape as ../m1-storage/build-pdnvme-test.nix):
#   /usr/lib/libhbcrash.dylib   DYLD_INSERT_LIBRARIES crash reporter (guest/hbcrash.c)
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  p = pd.packages.x86_64-linux;
  libSystem = import ../m1-userspace-diagnostics/build-libsystem-spawnattr.nix;
in pkgs.stdenv.mkDerivation {
  name = "darwin-lab-homebrew-guest-tools-arm64";
  dontUnpack = true;
  buildPhase = ''
    export DARWIN_SDK_ROOT=${p.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    ${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-clang \
      -std=gnu11 -Wall -Wextra -O1 -fno-omit-frame-pointer -dynamiclib \
      -isystem "$DARWIN_SDK_ROOT/usr/include" \
      ${./guest/hbcrash.c} \
      --ld-path=${p.native-ld}/bin/ld -nostdlib \
      -L${libSystem}/usr/lib -L${libSystem}/usr/lib/system \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains \
      -install_name /usr/lib/libhbcrash.dylib \
      -lSystem -o libhbcrash.dylib
  '';
  installPhase = ''
    mkdir -p "$out/usr/lib"
    cp libhbcrash.dylib "$out/usr/lib/"
  '';
  dontFixup = true;
  dontStrip = true;
}
