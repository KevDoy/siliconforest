# SPDX-License-Identifier: MIT
# The base disk image of the QEMU desktop loop: upstream's arm64 `virt` minimal image
# (GPT: ESP with loader, kernel collection and boot-args.txt; ext4 root) with
#   * the kernel collection of ./build-kc.nix (one kext patch: /dev/usb_hid_kbd for QEMU's
#     USB keyboard),
#   * the loader patched to honour pdgopmode=WxH (./loader-gop-mode.patch).
# Everything else, including the root's userland, is upstream's at the pinned commit.
# qd_image.py adds our userland and boot arguments to a copy of this image.
#   nix-build ... build-image.nix [--argstr kernelConfig debug] --no-out-link
# variant "wayland" starts from upstream's image-arm64-virt-wayland instead: the full userland
# without X11 (clang, git, curl, openssl, openssh, python, cmake, ...), a 3 GiB class root.
{ kernelConfig ? "release", variant ? "minimal" }:
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  p = pd.packages.x86_64-linux;
  base = if variant == "wayland" then (if kernelConfig == "debug" then p.image-arm64-virt-wayland-debug else p.image-arm64-virt-wayland)
    else if kernelConfig == "debug" then p.image-arm64-virt-minimal else p.image-arm64-virt-minimal-release;
in base.override (old: {
  kc = import ./build-kc.nix { inherit kernelConfig; };
  xnuLoader = old.xnuLoader.overrideAttrs (o: {
    patches = (o.patches or [ ]) ++ [ ./loader-gop-mode.patch ];
  });
})
