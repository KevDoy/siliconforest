# SPDX-License-Identifier: MIT
# pdvideo for the SiliconForest arm64 userland: one arm64 Mach-O, FFmpeg (build-ffmpeg.nix),
# libwayland-client and libffi linked statically; it needs libSystem and, for sound, the
# AudioToolbox and CoreAudio frameworks of ../../m1-sound/sysaudio (weak: without them it
# starts and plays without sound).
#
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes \
#     --no-out-link --cores 4 --max-jobs 1 build-pdvideo.nix
#
# Output: usr/bin/pdvideo (and usr/bin/ffmpeg, ffprobe of the same FFmpeg build, for staging
# from one path).
#
# `ffmpeg` is the FFmpeg build to link: pass the one with the pdavd hwaccel to get hardware
# decoding (pdvideo itself does not change):
#   import ./build-pdvideo.nix { ffmpeg = import ./build-ffmpeg.nix { extraPatches = [ ... ]; ... }; extraLibs = [ "-lpdavd" ]; }
{ ffmpeg ? import ./build-ffmpeg.nix {}, extraLdFlags ? [], extraLibs ? [], nameSuffix ? "" }:
let
  r = import ../../rebase/pd.nix;
  inherit (r) pd pkgs;
  lib = pkgs.lib;
  p = pd.packages.x86_64-linux;
  libSystem = import ../../libsystem-default/build-libsystem.nix;
  audiotoolbox = (import ../../m1-sound/sysaudio/build-sysaudio.nix).audiotoolbox;
  tc = p.arm64-cross-toolchain;
  triple = "arm64-apple-darwin20.4";
  sdk = "${p.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
  wl = p.wayland-arm64;
  ffi = p.libffi-arm64;
  proto = "${p.wayland-protocols}/share/wayland-protocols";
  fw = "System/Library/Frameworks";
in pkgs.stdenv.mkDerivation {
  name = "darwin-lab-pdvideo-arm64-darwin${nameSuffix}";
  src = ./src;
  nativeBuildInputs = [ pkgs.wayland-scanner ];
  buildPhase = ''
    for x in stable/xdg-shell/xdg-shell stable/viewporter/viewporter staging/fractional-scale/fractional-scale-v1 \
             unstable/xdg-decoration/xdg-decoration-unstable-v1; do
      n=$(basename $x)
      wayland-scanner client-header ${proto}/$x.xml $n-client-protocol.h
      wayland-scanner private-code ${proto}/$x.xml $n-protocol.c
    done
    mkdir -p cf-headers
    ln -s ${p.coreFoundation-arm64}/include cf-headers/CoreFoundation
    ${tc}/bin/${triple}-clang -std=gnu11 -O2 -Wall -Wextra -Wno-deprecated-declarations \
      -isysroot ${sdk} -mmacosx-version-min=26.5 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
      -I${libSystem}/usr/include -I. -Icf-headers -I${audiotoolbox}/usr/include \
      -I${ffmpeg}/usr/include -I${wl}/include \
      pdvideo.c ao_audioqueue.c *-protocol.c \
      --ld-path=${p.native-ld}/bin/ld -nostdlib -L${libSystem}/usr/lib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib \
      -Wl,-dylib_file,/${fw}/CoreAudio.framework/Versions/A/CoreAudio:${audiotoolbox}/${fw}/CoreAudio.framework/Versions/A/CoreAudio \
      -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains -L${p.coreFoundation-arm64}/usr/lib \
      ${ffmpeg}/usr/lib/libavformat.a ${ffmpeg}/usr/lib/libavcodec.a ${ffmpeg}/usr/lib/libswresample.a \
      ${ffmpeg}/usr/lib/libswscale.a ${ffmpeg}/usr/lib/libavutil.a \
      ${wl}/lib/libwayland-client.a ${ffi}/lib/libffi.a \
      -Wl,-weak_library,${audiotoolbox}/${fw}/AudioToolbox.framework/Versions/A/AudioToolbox \
      -Wl,-weak_library,${audiotoolbox}/${fw}/CoreAudio.framework/Versions/A/CoreAudio \
      ${lib.concatStringsSep " " (extraLdFlags ++ extraLibs)} -lSystem -o pdvideo
  '';
  installPhase = ''
    install -Dm755 pdvideo $out/usr/bin/pdvideo
    install -Dm755 ${ffmpeg}/usr/bin/ffmpeg $out/usr/bin/ffmpeg
    install -Dm755 ${ffmpeg}/usr/bin/ffprobe $out/usr/bin/ffprobe
    ${tc}/bin/${triple}-otool -L $out/usr/bin/pdvideo > $out/LINKAGE.txt
    cat $out/LINKAGE.txt
    if grep -v ':$' $out/LINKAGE.txt | grep -v '/usr/lib/libSystem.B.dylib\|AudioToolbox.framework\|CoreAudio.framework'; then
      echo "unexpected library reference" >&2; exit 1
    fi
  '';
  dontFixup = true;
  dontStrip = true;
}
