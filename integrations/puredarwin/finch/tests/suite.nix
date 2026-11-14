# SPDX-License-Identifier: MIT
# Source-based CLI comparison subset; blocked/GUI/Swift tests are not claimed.
let
  e = import ../environment.nix;
  inherit (e) pkgs finch;
  dropins = import ./build.nix;
  bt = import ../CoreBluetooth/tests.nix;
  tk = import ../CryptoTokenKit/tests.nix;
  od = import ../OpenDirectory/tests.nix;
  ck = import ../CloudKit/tests.nix;
  numeric = import ../Accelerate/build.nix { groups = [ "vDSP" "vMisc" "BLAS" "LAPACK" ]; };
  numericTests = import ../Accelerate/comparison.nix;
  network = import ../CFNetwork/build.nix;
  networkTests = import ../CFNetwork/tests.nix;
  entry = name: store: mode: extra: {
    inherit name mode;
    argv = [ "bin/${name}" ];
    timeout = 90;
  } // extra // { source = "${store}/bin/${name}"; };
  entries = [
    (entry "accounts-test" dropins "reference" { skip_lines = 1; })
    (entry "corebluetooth-test" bt "reference" { skip_lines = 1; })
    (entry "corebluetooth-unsupported-test" bt "expected" { expected = "expected/corebluetooth-unsupported-test.expected"; })
    (entry "cryptotokenkit-test" tk "reference" { skip_lines = 1; })
    (entry "cryptotokenkit-empty-test" tk "expected" { expected = "expected/cryptotokenkit-empty-test.expected"; })
    (entry "opendirectory-objects-test" od "reference" { skip_lines = 0; })
    (entry "cloudkit-test" ck "reference" { skip_lines = 1; })
    (entry "cloudkit-unavailable-test" ck "expected" { expected = "expected/cloudkit-unavailable-test.expected"; })
    (entry "cfnetwork-test" networkTests "reference" {
      skip_lines = 1; # First line reports the loaded CFNetwork image path.
      env = { DYLD_FRAMEWORK_PATH = "@ROOT@/System/Library/Frameworks"; };
    })
  ] ++ map (n: entry "${n}-test" numericTests "reference" {
    skip_lines = 0; # These numeric tests have no image-identification line.
    env = { DYLD_LIBRARY_PATH = "@ROOT@/lib"; };
  }) [ "vdsp" "vmisc" "blas" ];
  manifest = pkgs.writeText "finch-cli-manifest.json" (builtins.toJSON (map (x: builtins.removeAttrs x [ "source" ]) entries));
in pkgs.runCommand "siliconforest-finch-cli-conformance-arm64" { nativeBuildInputs = [ pkgs.python3 ]; } ''
  mkdir -p $out/bin $out/lib $out/expected $out/tools
  ${pkgs.lib.concatMapStringsSep "\n" (x: "cp ${x.source} $out/bin/${x.name}") entries}
  python3 ${../tools/portable-tests.py} ${../../homebrew/hb_macho.py} $out/bin $out/binary-hashes.json
  cp ${numeric}/staged-bootstrap/*.dylib $out/lib/
  cp -r ${network}/System $out/
  cp ${manifest} $out/manifest.json
  cp ${finch}/userland/tests/{corebluetooth-unsupported-test,cryptotokenkit-empty-test,cloudkit-unavailable-test}.expected $out/expected/
  cp ${finch}/tools/test-masks.txt ${finch}/tools/host-test-exceptions.txt $out/tools/
  cp ${../tools/compare-tests.py} $out/tools/compare-tests.py
  cp ${./README.md} $out/README.md
  mkdir -p $out/licenses
  cp ${finch}/LICENSE* $out/licenses/
''
