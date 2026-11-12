# SPDX-License-Identifier: MIT
# The lab's arm64 libSystem on the rebase target (SF_PD=tip, ./pd.nix): upstream's at the tip
# with the three patches of ../libsystem-default/build-libsystem.nix (the Homebrew stream's
# recipe, which names the pin and is not changed): the vm_page_size symbol, posix_spawnattr_init
# zeroing its struct, and the macOS ABI patch of GitHub issue 46. All three apply to the tip
# unchanged; upstream has none of them.
#   SF_PD=tip nix-build build-libsystem.nix --no-out-link
let
  source = import ./pd.nix;
in
assert source.tip || throw "rebase/build-libsystem.nix is the tip's libSystem: run it with SF_PD=tip";
source.pd.packages.x86_64-linux.libsystem-arm64.overrideAttrs (old: {
  pname = "puredarwin-libsystem-siliconforest";
  patches = (old.patches or [ ]) ++ [
    ../m1-userspace-diagnostics/fix-root/vm-page-size-symbol.patch
    ../m1-userspace-diagnostics/fix-spawnattr/posix-spawnattr-init-zero.patch
    ../libsystem-default/patches/macos-abi.patch
  ];
})
