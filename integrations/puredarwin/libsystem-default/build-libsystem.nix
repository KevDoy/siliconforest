# SPDX-License-Identifier: MIT
# The lab's default arm64 libSystem: what the M1 RAM root and the QEMU images stage as
# /usr/lib/libSystem.B.dylib.
#
#   upstream PureDarwin, arm64, at the revision of ../rebase/pd.nix (the tip; SF_PD=pin: e7aa00a6)
#   + ../m1-userspace-diagnostics/fix-root/vm-page-size-symbol.patch
#   + ../m1-userspace-diagnostics/fix-spawnattr/posix-spawnattr-init-zero.patch
#       (those two are the previous default, build-libsystem-spawnattr.nix)
#   + patches/macos-abi.patch (GitHub issue 46):
#       ___chkstk_darwin enters the real stack probe instead of landing on sandbox_check()
#       (every function with a frame over one page lost its first argument);
#       <fenv.h> with the macOS arm64 layout; the long double entry points;
#       getcontext/setcontext/makecontext/swapcontext; exports that programs built
#       against the macOS SDK import. See docs/roadmap/issues/SF-S-homebrew.md.
#   + patches/xpc-copy-description-never-aborts.patch (GitHub issue 51):
#       xpc_copy_description() describes a connection, data and a double, and names any
#       other type, instead of ending the process ("libxpc misuse: Unknown XPC type",
#       exit reason LIBXPC/2). Security's client describes its connection in every error
#       it builds, so any program whose SecItemCopyMatching failed was killed (the Claude
#       Code CLI: docs/roadmap/issues/SF-X-claude-code.md).
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-libsystem.nix
let
  pd = (import ../rebase/pd.nix).pd;
in pd.packages.x86_64-linux.libsystem-arm64.overrideAttrs (old: {
  pname = "puredarwin-libsystem-default";
  patches = (old.patches or []) ++ [
    ../m1-userspace-diagnostics/fix-root/vm-page-size-symbol.patch
    ../m1-userspace-diagnostics/fix-spawnattr/posix-spawnattr-init-zero.patch
    ./patches/macos-abi.patch
    ./patches/xpc-copy-description-never-aborts.patch
  ];
})
