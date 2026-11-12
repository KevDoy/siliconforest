# SPDX-License-Identifier: MIT
# The lab's default libSystem (../libsystem-default) plus upstream's newer sandbox profile
# compiler, taken from upstream's tip as one discrete patch (patches/0007, upstream commit
# 1ebd97cb, the libsandbox part): SBPL function definitions, (import ...), named profiles from
# /System/Library/Sandbox/Profiles. The official Firefox's process sandbox profiles use these;
# with the pin's compiler sandbox_init() rejects them ("define expects a name and a value").
# The kernel side (Sandbox.kext) is the same at the pin and the tip.
(import ../libsystem-default/build-libsystem.nix).overrideAttrs (old: {
  pname = "puredarwin-libsystem-cocoa";
  patches = (old.patches or [ ]) ++ [ ./patches/0007-tip-libsandbox-sbpl-functions-and-profiles.patch ];
})
