# SPDX-License-Identifier: MIT
# Private CGL using the installed Asahi EGL and a matching desktop GL dispatch.
let
  a = import ../cocoa/arm64.nix;
  inherit (a) pkgs pd;
  gl = import ./gl-api.nix;
  mesa = import ../m1-vulkan/mesa-darwin.nix;
  system = pd.libsystem-arm64;
  tc = pd.arm64-cross-toolchain;
  triple = "arm64-apple-darwin20.4";
  sdk = "${pd.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
in pkgs.stdenv.mkDerivation {
  pname = "siliconforest-firefox-cgl-asahi-arm64";
  version = "1";
  src = a.src;
  patches = [ ./cgl-patches/0001-native-asahi-egl.patch ];
  dontConfigure = true;
  buildPhase = ''
    F=src/Frameworks/OpenGL
    for name in CGL GLImaging; do
      ${tc}/bin/${triple}-clang -O2 -g -isysroot ${sdk} \
        -DPD_CGL_USE_EGL=1 -I$F/include -I${mesa}/usr/include \
        -I${system}/usr/include -c $F/$name.c -o $name.o
    done
    ${tc}/bin/${triple}-clang -dynamiclib -isysroot ${sdk} \
      -fuse-ld=${pd.native-ld}/bin/ld -nostdlib CGL.o GLImaging.o \
      -L${system}/usr/lib -lSystem \
      -Wl,-reexport_library,${gl}/lib/libGL.1.dylib \
      -Wl,-reexport_library,${a.glu}/usr/lib/libGLU.1.dylib \
      -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${system}/usr/lib/system/libdyld.dylib \
      -Wl,-platform_version,macos,26.5,26.5 \
      -Wl,-install_name,/private/var/db/sffx/gl-v1/Frameworks/OpenGL.framework/Versions/A/OpenGL \
      -o OpenGL
  '';
  installPhase = ''
    F=$out/Frameworks/OpenGL.framework
    mkdir -p $F/Versions/A $out/lib
    cp OpenGL $F/Versions/A/
    ln -s A $F/Versions/Current
    ln -s Versions/Current/OpenGL $F/OpenGL
    cp -L ${gl}/lib/libGL.1.dylib $out/lib/
    ln -s libGL.1.dylib $out/lib/libGL.dylib
    cp -L ${a.glu}/usr/lib/libGLU.1.dylib $out/lib/
  '';
  dontFixup = true;
  dontStrip = true;
}
