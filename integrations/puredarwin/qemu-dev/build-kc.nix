# SPDX-License-Identifier: MIT
# The kernel collection of the QEMU desktop loop: upstream's arm64 `virt` collection (RELEASE
# kernel by default) with the boot kexts rebuilt from a patched tree. The one patch makes
# /dev/usb_hid_kbd appear for QEMU's USB keyboard (see pdhid-publish-keyboard.patch).
#   nix-build ... build-kc.nix [--argstr kernelConfig debug] --no-out-link   ->  <out>/kernel
{ kernelConfig ? "release" }:
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  p = pd.packages.x86_64-linux;
  kexts = p.kexts-arm64.overrideAttrs (old: {
    src = pkgs.applyPatches {
      name = "puredarwin-qemu-dev-kexts-source";
      src = old.src;
      patches = [ ./pdhid-publish-keyboard.patch ];
    };
  });
  kc = if kernelConfig == "debug" then p.kc-arm64-debug else p.kc-arm64;
in kc.override { inherit kexts; }
