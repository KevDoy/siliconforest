# SPDX-License-Identifier: MIT
# The system audio path of the PureDarwin M1 guest, arm64 Mach-O, from the pinned tree:
#   coreaudiod    the tree's audio server with ./coreaudiod-dev-backend.patch (plays through the
#                 device nodes of PDAppleSound when there is no IOAudioEngine)
#   audiotoolbox  the tree's CoreAudio, AudioToolbox and AudioUnit frameworks, unchanged
#   pcmplay       the tree's test player, unchanged
#   tools         ./user: pdtone (a tone through an AudioQueue) and pdvolume (the volume keys' command)
#   all           the four joined as one root
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-sysaudio.nix -A all
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  p = pd.packages.x86_64-linux;
  libSystem = import ../../m1-userspace-diagnostics/build-libsystem-spawnattr.nix;
  common = {
    darwinCrossToolchain = p.arm64-cross-toolchain;
    nativeLd = p.native-ld;
    appleSdk = p.apple-sdk;
    inherit libSystem;
    corefoundation = p.coreFoundation-arm64;
    targetTriple = "arm64-apple-darwin20.4";
  };
  src = pd.outPath;
  sdk = "${p.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  fw = "System/Library/Frameworks";
in rec {
  audiotoolbox = pkgs.callPackage "${src}/nix/pkgs/apple/audiotoolbox.nix" (common // {
    src = "${src}/src/Frameworks/AudioToolbox";
    coreAudioSrc = "${src}/src/Frameworks/CoreAudio";
  });

  pcmplay = pkgs.callPackage "${src}/nix/pkgs/apple/pcmplay.nix" (common // {
    inherit audiotoolbox;
    src = "${src}/src/Userspace/pcmplay";
  });

  coreaudiod = (pkgs.callPackage "${src}/nix/pkgs/apple/coreaudiod.nix" (common // { iokit = p.iokit-arm64; })).overrideAttrs (old: {
    pname = "puredarwin-coreaudiod-dev";
    # the recipe's source is the tree's src directory, the patch is against the tree
    src = pkgs.runCommand "puredarwin-coreaudiod-dev-source" { nativeBuildInputs = [ pkgs.patch ]; } ''
      cp -R ${old.src} "$out"
      chmod -R u+w "$out"
      patch -d "$out" -p2 < ${./coreaudiod-dev-backend.patch}
    '';
  });

  tools = pkgs.stdenv.mkDerivation {
    name = "darwin-lab-sysaudio-tools-arm64";
    dontUnpack = true;
    buildPhase = ''
      CC=${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-clang
      mkdir -p cf-headers
      ln -s ${p.coreFoundation-arm64}/include cf-headers/CoreFoundation
      for t in pdtone pdvolume; do
        $CC -isysroot ${sdk} -mmacosx-version-min=26.5 -std=c11 -O2 -Wall -Wextra -Icf-headers -I${libSystem}/usr/include \
          -I${audiotoolbox}/usr/include -c ${./user}/$t.c -o $t.o
        $CC -isysroot ${sdk} -mmacosx-version-min=26.5 -fuse-ld=${p.native-ld}/bin/ld -nostdlib \
          -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
          -Wl,-dylib_file,/${fw}/CoreAudio.framework/Versions/A/CoreAudio:${audiotoolbox}/${fw}/CoreAudio.framework/Versions/A/CoreAudio \
          -Wl,-platform_version,macos,26.5,26.5 -L${libSystem}/usr/lib -L${p.coreFoundation-arm64}/usr/lib \
          $t.o ${audiotoolbox}/${fw}/AudioToolbox.framework/Versions/A/AudioToolbox \
          ${audiotoolbox}/${fw}/CoreAudio.framework/Versions/A/CoreAudio -lCoreFoundation -lSystem -o $t
      done
    '';
    installPhase = ''
      install -Dm755 pdtone "$out/usr/bin/pdtone"
      install -Dm755 pdvolume "$out/usr/bin/pdvolume"
    '';
    dontFixup = true;
    dontStrip = true;
  };

  all = pkgs.runCommand "darwin-lab-sysaudio-arm64" {} ''
    mkdir -p "$out/usr/sbin" "$out/usr/bin" "$out/${fw}"
    cp ${coreaudiod}/usr/sbin/coreaudiod "$out/usr/sbin/"
    cp ${pcmplay}/bin/pcmplay ${tools}/usr/bin/pdtone ${tools}/usr/bin/pdvolume "$out/usr/bin/"
    for f in CoreAudio AudioToolbox AudioUnit; do
      install -Dm755 ${audiotoolbox}/${fw}/$f.framework/Versions/A/$f "$out/${fw}/$f.framework/Versions/A/$f"
    done
  '';
}
