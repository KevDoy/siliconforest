# SPDX-License-Identifier: MIT
# Reuse the actual ioreg cross SDK, CoreFoundation and MIG-backed IOKit recipe.
let
 source=import ../rebase/pd.nix;
 inherit (source) pkgs pd;
 base=pd.packages.x86_64-linux.ioreg-arm64;
in base.overrideAttrs (old: {
 pname="puredarwin-cio-snapshot";
 src=pkgs.runCommand "cio-snapshot-source" {} ''
  mkdir -p "$out"
  cp ${./tools/pd-cio-snapshot.c} "$out/ioreg.c"
  touch "$out/term_stub.c"
 '';
 installPhase=''
  mkdir -p "$out/bin"
  cp ioreg "$out/bin/pd-cio-snapshot"
 '';
})
