# SPDX-License-Identifier: MIT
# The lab's default arm64 libSystem: what the M1 RAM root and the QEMU images stage as
# /usr/lib/libSystem.B.dylib.
#
#   baseline e7aa00a6 (upstream PureDarwin, arm64)
#   + ../m1-userspace-diagnostics/fix-root/vm-page-size-symbol.patch
#   + ../m1-userspace-diagnostics/fix-spawnattr/posix-spawnattr-init-zero.patch
#       (those two are the previous default, build-libsystem-spawnattr.nix)
#   + patches/macos-abi.patch (GitHub issue 46):
#       ___chkstk_darwin enters the real stack probe instead of landing on sandbox_check()
#       (every function with a frame over one page lost its first argument);
#       <fenv.h> with the macOS arm64 layout; the long double entry points;
#       getcontext/setcontext/makecontext/swapcontext; exports that programs built
#       against the macOS SDK import. See docs/roadmap/issues/SF-S-homebrew.md.
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-libsystem.nix
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
in pd.packages.x86_64-linux.libsystem-arm64.overrideAttrs (old: {
  pname = "puredarwin-libsystem-default";
  patches = (old.patches or []) ++ [
    ../m1-userspace-diagnostics/fix-root/vm-page-size-symbol.patch
    ../m1-userspace-diagnostics/fix-spawnattr/posix-spawnattr-init-zero.patch
    ./patches/macos-abi.patch
  ];
})
