# Mesa (omacom integration/aurora 147a62f6190) for the PureDarwin M1 guest: arm64 Mach-O, Gallium
# asahi + softpipe, EGL (surfaceless only) and GLES2/3 on PureDarwin's /dev/dri libdrm.  No LLVM,
# X11, Wayland or GBM in the closure.  libagx's OpenCL C is compiled by the native build's tools.
#
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes mesa-darwin.nix
let
  c = import ./common.nix;
  inherit (c) pkgs p triple sdkRoot;
  lib = pkgs.lib;
  nativeTools = import ./mesa-native.nix;
  libdrm = p.libdrm-dri-arm64;
  zlib = p.zlib-arm64;
  expat = p.expat-arm64;
  tc = p.arm64-cross-toolchain;
  libSystem = p.libsystem-arm64;
  cxx = p.libcxxDylib-arm64;
  cxxabi = p.libcxxabiDylib-arm64;
  cf = p.coreFoundation-arm64;
  iokit = p.iokit-arm64;

  q = s: "'${s}'";
  list = xs: lib.concatMapStringsSep ", " q xs;
  common = [ "-isysroot" sdkRoot "-mmacosx-version-min=26.5" ];
  cflags = common ++ [
    "-Qunused-arguments" "-U_FORTIFY_SOURCE" "-D_FORTIFY_SOURCE=0" "-fno-stack-protector"
    "-I${libSystem}/usr/include"
    "-I${libdrm}/include" "-I${libdrm}/include/libdrm"
    "-I${lib.getDev zlib}/include" "-I${lib.getDev expat}/include"
  ];
  ldflags = common ++ [
    "-fuse-ld=${p.native-ld}/bin/ld" "-nostdlib" "-L${libSystem}/usr/lib"
    "-L${libdrm}/lib" "-L${zlib}/lib" "-L${expat}/lib"
    "-L${cf}/usr/lib" "-L${iokit}/usr/lib" "-lIOKitCF" "-lCoreFoundation"
    "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib"
    "-Wl,-platform_version,macos,26.5,26.5" "-Wl,-fixup_chains"
  ];
in pkgs.stdenv.mkDerivation {
  pname = "darwin-lab-mesa-asahi-arm64-darwin";
  version = c.mesaVersion;
  src = c.mesaSrc;

  nativeBuildInputs = with pkgs; [ meson ninja pkg-config c.pythonEnv bison flex glslang ];

  patches = [
    ./patches/0001-meson-darwin-is-a-drm-system.patch
    ./patches/0002-asahi-no-macos-iokit-wrap-tooling.patch
    ./patches/0003-os-file-same-file-via-F_PD_SAMEFILE.patch
    ./patches/0004-u-screen-query-DRM_CAP_PRIME-on-darwin.patch
    ./patches/0005-asahi-ioc-field-accessors-for-bsd-ioccom.patch
  ];

  postPatch = ''
    patchShebangs .
    # Darwin has no <sys/sysmacros.h>; major()/minor()/makedev() come from <sys/types.h>.  The
    # values are XNU's: 8-bit major, 24-bit minor (PDDRMDri puts the open slot above the low byte).
    mkdir -p pd-compat/sys
    cp ${./compat/sys/sysmacros.h} pd-compat/sys/sysmacros.h
  '';

  configurePhase = ''
    runHook preConfigure
    export PATH="${nativeTools}/bin:${p.native-ld}/bin:$PATH"
    export PKG_CONFIG_PATH="${libdrm}/lib/pkgconfig:${lib.getDev zlib}/lib/pkgconfig:${lib.getDev expat}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="$PKG_CONFIG_PATH"

    cat > puredarwin-cross.ini <<EOF
    [binaries]
    c = '${tc}/bin/${triple}-clang'
    cpp = '${tc}/bin/${triple}-clang++'
    objc = '${tc}/bin/${triple}-clang'
    objcpp = '${tc}/bin/${triple}-clang++'
    ar = '${tc}/bin/${triple}-ar'
    strip = '${tc}/bin/${triple}-strip'
    pkg-config = '${pkgs.pkg-config}/bin/pkg-config'
    install_name_tool = '${tc}/bin/${triple}-install_name_tool'

    [built-in options]
    c_args = [${list cflags}, '-I$PWD/pd-compat']
    cpp_args = [${list cflags}, '-I$PWD/pd-compat', '-nostdinc++', '-I${cxx}/usr/include/c++/v1']
    objc_args = [${list cflags}, '-I$PWD/pd-compat']
    c_link_args = [${list ldflags}, '-lSystem']
    objc_link_args = [${list ldflags}, '-lSystem']
    cpp_link_args = [${list ldflags}, '-L${cxx}/usr/lib', '-L${cxxabi}/usr/lib', '-lc++', '-lc++abi', '-lSystem']

    [host_machine]
    system = 'darwin'
    subsystem = 'macos'
    cpu_family = 'aarch64'
    cpu = 'aarch64'
    endian = 'little'

    [properties]
    needs_exe_wrapper = true
    EOF
    sed -i 's/^    //' puredarwin-cross.ini

    meson setup build \
      --cross-file puredarwin-cross.ini \
      --prefix=$out/usr \
      --libdir=lib \
      --buildtype=release \
      -Ddefault_library=shared \
      -Dgallium-drivers=asahi,softpipe \
      -Dvulkan-drivers= \
      -Dplatforms= \
      -Degl-native-platform=surfaceless \
      -Dopengl=true \
      -Dgles1=disabled \
      -Dgles2=enabled \
      -Dglx=disabled \
      -Degl=enabled \
      -Dgbm=disabled \
      -Dllvm=disabled \
      -Ddraw-use-llvm=false \
      -Dmesa-clc=system \
      -Dprecomp-compiler=system \
      -Dgallium-va=disabled \
      -Dgallium-rusticl=false \
      -Dglvnd=disabled \
      -Dlmsensors=disabled \
      -Dzstd=disabled \
      -Dvalgrind=disabled \
      -Dlibunwind=disabled \
      -Dxlib-lease=disabled \
      -Dbuild-tests=false
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    ninja -C build install
    # image layout: everything under /usr/lib, like PureDarwin's own Mesa packages
    INT=${tc}/bin/${triple}-install_name_tool
    dylibs=$(find "$out/usr/lib" -maxdepth 1 -name "*.dylib" -not -type l)
    for d in $dylibs; do "$INT" -id "/usr/lib/$(basename "$d")" "$d" || true; done
    for f in $dylibs; do
      for d in $dylibs; do
        b=$(basename "$d")
        "$INT" -change "@rpath/$b" "/usr/lib/$b" "$f" 2>/dev/null || true
        "$INT" -change "$out/usr/lib/$b" "/usr/lib/$b" "$f" 2>/dev/null || true
      done
    done
    runHook postInstall
  '';

  dontFixup = true;
  dontStrip = true;
}
