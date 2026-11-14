# SPDX-License-Identifier: MIT
# Default target is the whole Finch Accelerate tree. groups permits a staged
# bootstrap while we author the missing open declarations; never install that as whole.
{ groups ? [ "vDSP" "vMisc" "BLAS" "LAPACK" "vImage" "Quadrature" "LinearAlgebra" "SparseBLAS" "Sparse" ] }:
let
  e = import ../environment.nix;
  inherit (e) pkgs p finch libsystem sdk cc ld;
in pkgs.stdenv.mkDerivation {
  name = "siliconforest-finch-Accelerate-arm64";
  src = finch;
  nativeBuildInputs = [ pkgs.python3 ];
  dontConfigure = true;
  buildPhase = ''
    export DARWIN_SDK_ROOT=${sdk}
    cp -r ${./include} headers
    chmod -R u+w headers
    CF="-isysroot ${sdk} -mmacosx-version-min=26.5 -O2 -fblocks -std=c17 -D_DARWIN_C_SOURCE -I$PWD/headers -I${libsystem}/usr/include -I${p.libobjc-arm64}/usr/include -I${p.coreFoundation-arm64}/include -I${p.coregraphics-arm64}/usr/include -I${p.onyx2d-arm64}/usr/include -I${e.pd.outPath}/src/Frameworks"
    LF="--ld-path=${ld} -nostdlib -isysroot ${sdk} -L${libsystem}/usr/lib -L${libsystem}/usr/lib/system -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libsystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,26.5,26.5"
    mkdir -p objects libs
    for group in ${pkgs.lib.concatStringsSep " " groups}; do
      mkdir -p objects/$group
      for s in userland/Accelerate/$group/*.c; do
        ${cc} $CF -DSF_COMPONENT_$group -DSF_ACCELERATE_PREPROCESS -E -P "$s" -o objects/$group/$(basename "$s" .c).i
      done
      python3 ${./declarations.py} ${../tools/c_functions.py} headers/Accelerate/$group-generated.h objects/$group/*.i
      if [ "$group" = BLAS ]; then
        python3 ${./blas-declarations.py} headers/Accelerate/BLAS-generated.h objects/BLAS/abi-input
        python3 userland/Accelerate/BLAS/gen-abi.py objects/BLAS/abi-input objects/BLAS/abi-input/symbols > objects/BLAS/abi.c
        ${cc} $CF -c objects/BLAS/abi.c -o objects/BLAS/abi.o
      fi
      for s in userland/Accelerate/$group/*.c; do
        ${cc} $CF -DSF_COMPONENT_$group -c "$s" -o objects/$group/$(basename "$s" .c).o
      done
      if [ "$group" = vMisc ]; then
        ${cc} $CF -c ${./math-compat.c} -o objects/$group/math-compat.o
      fi
      ${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-nm -g -U objects/$group/*.o | awk '{print $NF}' | sort -u | sed -n '/^_/p' | sed '/^_finch_/d; /^_vimage_/d; /^_sparse_report$/d; /^___\(sin\|cos\|tan\)pi/d' > objects/$group/exports
      ${cc} -dynamiclib objects/$group/*.o $LF -Wl,-exported_symbols_list,objects/$group/exports ${p.compiler-rt-arm64}/lib/libcompiler_rt.a -install_name /System/Library/Frameworks/Accelerate.framework/Versions/A/Frameworks/vecLib.framework/Versions/A/lib$group.dylib -lSystem -o libs/lib$group.dylib
    done
  '';
  installPhase = ''
    mkdir -p $out/staged-bootstrap $out/usr/share/licenses/finch-Accelerate
    cp libs/*.dylib $out/staged-bootstrap/
    cp -r headers $out/
    cp ${finch}/LICENSE* $out/usr/share/licenses/finch-Accelerate/
  '';
  dontFixup = true;
  dontStrip = true;
}
