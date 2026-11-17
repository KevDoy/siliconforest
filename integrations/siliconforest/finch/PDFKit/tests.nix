# SPDX-License-Identifier: MIT
# File-only public PDF probe; no PDFView, printing, dummy library or SDK.
let
  e = import ../environment.nix;
  inherit (e) pkgs p finch libsystem sdk cc ld;
  appkit = p.appkit-msm-arm64;
in pkgs.stdenv.mkDerivation {
  name = "siliconforest-finch-PDFKit-documents-tests-arm64";
  dontUnpack = true;
  dontConfigure = true;
  buildPhase = ''
    export DARWIN_SDK_ROOT=${sdk}
    CF="-isysroot ${sdk} -mmacosx-version-min=26.5 -O2 -fblocks -fobjc-exceptions -fno-objc-arc -D_DARWIN_C_SOURCE -Wno-nullability-completeness -Wno-macro-redefined -Wno-incomplete-implementation -I${./include} -I${appkit}/usr/include -I${p.foundation-arm64}/usr/include -I${p.coreFoundation-arm64}/include -I${p.libobjc-arm64}/usr/include -I${p.coregraphics-arm64}/usr/include -I${p.coretext-arm64}/usr/include -I${p.onyx2d-arm64}/usr/include -I${e.pd.outPath}/src/Frameworks -I${e.pd.outPath}/src/Frameworks/OpenGL/include -I${p.mesa-msm-arm64}/usr/include"
    ${cc} $CF -c ${./probe.m} -o probe.o
    ${cc} probe.o --ld-path=${ld} -nostdlib -isysroot ${sdk} -L${libsystem}/usr/lib -L${libsystem}/usr/lib/system -L${p.foundation-arm64}/usr/lib -L${p.coreFoundation-arm64}/usr/lib -L${p.libobjc-arm64}/usr/lib -L${p.coregraphics-arm64}/usr/lib -L${p.coretext-arm64}/usr/lib -lCoreGraphics -lCoreText -lFoundation -lCoreFoundation -lobjc -lSystem -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:${libsystem}/usr/lib/system/libdyld.dylib -Wl,-platform_version,macos,26.5,26.5 -o pdfkit-document-probe
  '';
  installPhase = ''mkdir -p $out/bin; cp pdfkit-document-probe $out/bin/'';
  dontFixup = true;
  dontStrip = true;
}
