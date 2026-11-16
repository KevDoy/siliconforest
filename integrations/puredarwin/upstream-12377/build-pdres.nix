# SPDX-License-Identifier: MIT
# Port the small upstream recipe; no upstream flake/Nix reorganisation.
let
  inherit (import ../m1-rust-kext/pins.nix) pkgs pd crossToolchain appleSdk;
  source = pkgs.applyPatches {
    name = "puredarwin-pdres-selected-source";
    src = pd.outPath;
    patches = [ ./patches/0163b2db-pdres.patch ];
  };
  p = pd.packages.x86_64-linux;
in pkgs.callPackage (source + "/nix/pkgs/apps/pdres.nix") {
  darwinCrossToolchain = crossToolchain;
  nativeLd = p.native-ld;
  libSystem = p.libsystem-arm64;
  libobjc = p.libobjc-arm64;
  corefoundation = p.coreFoundation-arm64;
  pdsurface = p.pdsurface-arm64;
  coregraphics = p.coregraphics-arm64;
  inherit appleSdk;
  src = source + "/src/Userspace/pdres";
  targetTriple = "arm64-apple-darwin20.4";
}
