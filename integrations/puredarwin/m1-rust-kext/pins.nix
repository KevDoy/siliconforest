# The pinned PureDarwin flake (same pin as every other recipe in this lab) and its nixpkgs.
# Everything here, including the Rust toolchain, comes out of these two.
rec {
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  # arm64-apple-darwin20.4-{clang,ld,nm,otool,objdump}: what the tree's kexts are built with
  crossToolchain = pd.packages.x86_64-linux.arm64-cross-toolchain;
  appleSdk = pd.packages.x86_64-linux.apple-sdk;
}
