# SPDX-License-Identifier: MIT
# libSystem for running programs built against the macOS SDK (Homebrew bottles): the lab's
# canonical recipe (../m1-userspace-diagnostics/build-libsystem-spawnattr.nix: baseline
# e7aa00a6 + vm_page_size + posix_spawnattr) plus patches/libsystem-homebrew.patch.
# See docs/roadmap/issues/SF-S-homebrew.md for what the patch contains and why.
let
  base = import ../m1-userspace-diagnostics/build-libsystem-spawnattr.nix;
in base.overrideAttrs (old: {
  pname = "puredarwin-libsystem-homebrew";
  patches = (old.patches or []) ++ [ ./patches/libsystem-homebrew.patch ];
})
