# The Rust side of PDRustHello.kext, in the three steps rust/toolchain.nix defines:
#   sysroot  core + compiler_builtins + alloc for aarch64-apple-xnukext (built once)
#   crates   pd_xnu (rlib: allocator, logger, panic handler) and pdrusthello (staticlib)
#   object   pdrusthello.o, the staticlib prelinked with only the C entry points exported
# Phase B adds its crates (bindings, kernel, drivers) as further mkCrate calls here.
#   nix-build build-rust.nix -A object     (also: -A sysroot, -A pdrusthello)
let
  pins = import ./pins.nix;
  rust = import ./rust/toolchain.nix { inherit (pins) pd pkgs crossToolchain; };
in rec {
  inherit rust;
  inherit (rust) sysroot;
  pd_xnu = rust.mkCrate { crateName = "pd_xnu"; src = ./rust/pd_xnu; };
  pdrusthello = rust.mkCrate {
    crateName = "pdrusthello";
    crateType = "staticlib";
    src = ./rust/pdrusthello;
    deps = [ pd_xnu ];
  };
  object = rust.mkKextObject {
    name = "pdrusthello";
    staticlib = pdrusthello;
    # the functions declared in kext/pd_rust_hello.h; everything else becomes local
    exports = [ "_pd_rust_hello_entry" "_pd_rust_hello_calls" ];
  };
}
