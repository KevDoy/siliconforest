let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
in pkgs.callPackage ../m1-display/kc-display.nix {
  inherit pkgs;
  kcTools = import ../m1-bringup/kc-tools-fixed.nix;
  kernel = /nix/store/1ys1x1hzkq3v0l8bc14v2bq1jm5i46zn-puredarwin-kernel-arm64-t8103-bringup-0.1;
  kernelSource = /nix/store/274mmx792npkzabn9yb7hg17q58syyn9-source;
  genericKexts = pd.packages.x86_64-linux.kexts-arm64;
  platformKexts = /nix/store/7s13ygyk7av9b5ryydvqzx13x82sbiph-puredarwin-m1-bringup-platform-0.1;
  displayKexts = import ./build-framebuffer.nix;
  kcRecipe = pd.outPath + "/nix/pkgs/toolchain/kc-arm64.nix";
}
