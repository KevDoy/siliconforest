# SPDX-License-Identifier: MIT
# The tree's arm64 IOKit (libIOKitCF.dylib, which IOKit.framework/Versions/A/IOKit links to)
# relinked with guest/iokit_main_port.c (kIOMainPortDefault). Same link line otherwise.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  p = pd.packages.x86_64-linux;
in p.iokit-arm64.overrideAttrs (old: {
  pname = "puredarwin-iokit-homebrew";
  preBuild = ''
    ${p.arm64-cross-toolchain}/bin/arm64-apple-darwin20.4-clang -c ${./guest/iokit_main_port.c} -o iokit_main_port.o
  '';
  buildPhase = builtins.replaceStrings [ "-lobjc -lFoundation" ] [ "iokit_main_port.o -lobjc -lFoundation" ] old.buildPhase;
})
