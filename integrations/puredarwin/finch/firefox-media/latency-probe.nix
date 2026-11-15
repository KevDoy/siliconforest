# SPDX-License-Identifier: MIT
let e=import ../environment.nix;unit=import ../AudioToolbox/units/build.nix; in
e.pkgs.runCommand "siliconforest-firefox-media-latency-probe-arm64" {} ''
 export DARWIN_SDK_ROOT=${e.sdk}
 mkdir -p $out/bin $out/lib
 ${e.cc} -O2 -std=c11 -D_DARWIN_C_SOURCE -isysroot ${e.sdk} \
  -I${unit}/usr/include -I${e.libsystem}/usr/include -I${e.p.coreFoundation-arm64}/include \
  ${./latency-probe.c} --ld-path=${e.ld} -nostdlib -L${e.libsystem}/usr/lib \
  -F${unit}/System/Library/Frameworks -framework AudioUnit -framework CoreAudio -lSystem \
  -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${e.libsystem}/usr/lib/system/libdyld.dylib \
  -Wl,-platform_version,macos,26.5,26.5 -o $out/bin/audio-latency-probe
''
