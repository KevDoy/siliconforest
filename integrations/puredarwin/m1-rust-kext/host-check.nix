# Runs the Rust side of PDRustHello on the build host (x86_64 Linux, the same rustc and
# sources, libc behind the kernel imports; see host-check/host.c) and fails unless the
# value in kext/pd_rust_hello.h (PD_RUST_HELLO_EXPECT) is what the code returns.
# $out/log is the text the kext is expected to log. Not evidence about the arm64 build.
let
  inherit (import ./pins.nix) pkgs;
in pkgs.stdenv.mkDerivation {
  name = "pdrusthello-host-check";
  dontUnpack = true;
  nativeBuildInputs = [ pkgs.rustc.unwrapped ];
  buildPhase = ''
    R="rustc --edition=2024 -Copt-level=2 -Cpanic=abort -Ccodegen-units=1"
    $R --crate-type rlib --crate-name pd_xnu ${./rust/pd_xnu}/lib.rs \
      --remap-path-prefix=${./rust/pd_xnu}=pd_xnu
    $R --crate-type staticlib --crate-name pdrusthello ${./rust/pdrusthello}/lib.rs \
      --remap-path-prefix=${./rust/pdrusthello}=pdrusthello --extern pd_xnu=libpd_xnu.rlib
    cc -O1 -Wall -Werror -I${./kext} ${./host-check/host.c} libpdrusthello.a -o host
    mkdir -p $out
    ./host | tee $out/log
  '';
  dontInstall = true;
}
