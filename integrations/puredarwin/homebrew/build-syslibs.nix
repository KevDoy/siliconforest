# SPDX-License-Identifier: MIT
# The system libraries macOS ships in /usr/lib that Homebrew bottles link instead of
# bringing their own (`uses_from_macos`), as dylibs with the install names and
# compatibility versions the bottles recorded. The code is the PureDarwin tree's own arm64
# builds of the upstream libraries (static archives there); this recipe only links each
# archive into a dylib against the lab's libSystem (./build-libsystem.nix), the way the
# tree's nix/pkgs/base/libz-dylib.nix does for zlib.
#
#   libncurses.5.4  ncurses 6.6 (wide build). macOS ships ncurses 6.0 under this name. The
#                   termcap/terminfo entry points (what readline, less, vim, tmux use) are
#                   the same; full curses programs built against Apple's header see
#                   upstream's struct layouts, which is the documented ABI 6 but untested here.
#   libpanel.5.4    from the same build
#   libxml2.2, libexpat.1, libffi, libsqlite3      current upstream releases
#   libbz2.1.0      bzip2 1.0.8, compiled here from upstream's sources
#   libutil         macOS keeps a few BSD helpers here (humanize_number, pidfile_*, ...);
#                   the surveyed bottles link it and import nothing from it. This one
#                   exports nothing: a program that does import from it fails at launch
#                   with dyld's missing-symbol message rather than getting a fake.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  p = pd.packages.x86_64-linux;
  libSystem = import ./build-libsystem.nix;
  cc = "${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-clang";
  link = "${cc} -dynamiclib -nostdlib --ld-path=${p.native-ld}/bin/ld -L${libSystem}/usr/lib -L${libSystem}/usr/lib/system "
       + "-Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libSystem}/usr/lib/system/libdyld.dylib "
       + "-Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains -L${p.libzDylib-arm64}/usr/lib -L${p.libiconv-arm64}/usr/lib -L. ";
  # name, archive, compatibility version, current version, extra libraries
  lib = name: archive: compat: current: extra: ''
    ${link} -install_name /usr/lib/${name}.dylib -compatibility_version ${compat} -current_version ${current} \
      -Wl,-all_load ${archive} -lSystem ${extra} -o ${name}.dylib
  '';
in pkgs.stdenv.mkDerivation {
  name = "darwin-lab-homebrew-syslibs-arm64";
  dontUnpack = true;
  buildPhase = ''
    ${lib "libncurses.5.4" "${p.ncurses-arm64}/lib/libncursesw.a" "5.4.0" "5.4.0" ""}
    ${lib "libpanel.5.4" "${p.ncurses-arm64}/lib/libpanelw.a" "5.4.0" "5.4.0" "-lncurses.5.4"}
    ${lib "libexpat.1" "${p.expat-arm64}/lib/libexpat.a" "7.0.0" "10.0.0" ""}
    ${lib "libffi" "${p.libffi-arm64}/lib/libffi.a" "1.0.0" "40.0.0" ""}
    ${lib "libsqlite3" "${p.sqlite-arm64}/lib/libsqlite3.a" "9.0.0" "360.0.0" "-lz.1"}
    ${lib "libxml2.2" "${p.libxml2-arm64}/lib/libxml2.a" "10.0.0" "10.9.0" "-lz.1 -liconv.2"}
    # libbz2 1.0.8 from upstream's sources (the tree has no arm64 bzip2)
    mkdir bz && tar xf ${pkgs.bzip2.src} -C bz --strip-components=1
    for f in blocksort huffman crctable randtable compress decompress bzlib; do
      ${cc} -O2 -D_FILE_OFFSET_BITS=64 -isysroot ${p.apple-sdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
        -I${libSystem}/usr/include -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector -c bz/$f.c -o bz/$f.o
    done
    ${link} -install_name /usr/lib/libbz2.1.0.dylib -compatibility_version 1.0.0 -current_version 1.0.8 bz/*.o -lSystem -o libbz2.1.0.dylib
    echo 'static int unused __attribute__((unused));' > empty.c
    ${cc} -c empty.c -o empty.o
    ${link} -install_name /usr/lib/libutil.dylib -compatibility_version 1.0.0 -current_version 1.0.0 empty.o -lSystem -o libutil.dylib
  '';
  installPhase = ''
    mkdir -p $out/usr/lib
    cp *.dylib $out/usr/lib/
    cd $out/usr/lib
    ln -s libncurses.5.4.dylib libncurses.dylib; ln -s libncurses.5.4.dylib libcurses.dylib; ln -s libncurses.5.4.dylib libtermcap.dylib
    ln -s libpanel.5.4.dylib libpanel.dylib
    ln -s libbz2.1.0.dylib libbz2.dylib
    ln -s libexpat.1.dylib libexpat.dylib; ln -s libxml2.2.dylib libxml2.dylib
  '';
  dontFixup = true;
  dontStrip = true;
}
