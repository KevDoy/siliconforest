# SPDX-License-Identifier: MIT
# The default libSystem (../libsystem-default/build-libsystem.nix) and one patch for Apple's
# Activity Monitor (GitHub issue 53, docs/roadmap/issues/SF-AA-activity-monitor.md):
#   libsystem-memorystatus-level.patch  memorystatus_get_level(), the wrapper of the kernel's
#                                       system call 453 (the memory-pressure level)
# Until it is folded into the default recipe (a core file: QEMU rehearsal and guarded install,
# docs/roadmap/issues/SF-Y-terminal-app.md "Installing"), Activity Monitor's stage loads this
# libSystem through DYLD_LIBRARY_PATH.
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-libsystem-am.nix
(import ../libsystem-default/build-libsystem.nix).overrideAttrs (old: {
  pname = "puredarwin-libsystem-actmon";
  patches = (old.patches or []) ++ [ ./libsystem-memorystatus-level.patch ];
})
