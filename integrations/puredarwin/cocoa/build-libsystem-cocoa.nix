# SPDX-License-Identifier: MIT
# The lab's default libSystem (../libsystem-default) plus upstream's newer sandbox profile
# compiler, taken from upstream's tip as one discrete patch (patches/0007, upstream commit
# 1ebd97cb, the libsandbox part): SBPL function definitions, (import ...), named profiles from
# /System/Library/Sandbox/Profiles. The official Firefox's process sandbox profiles use these;
# with the pin's compiler sandbox_init() rejects them ("define expects a name and a value").
# The kernel side (Sandbox.kext) is the same at the pin and the tip.
(import ../libsystem-default/build-libsystem.nix).overrideAttrs (old: {
  pname = "puredarwin-libsystem-cocoa";
  patches = (old.patches or [ ]) ++ [
    ./patches/0007-tip-libsandbox-sbpl-functions-and-profiles.patch
    # ours, on top of the tip's compiler: operation names as values, string functions, numbers
    # and comparisons, when/unless, defined?, a profile's own function as a filter, and
    # require-all / require-not / with-filter compiled to the strict side (what cannot be
    # checked makes a deny rule broader and leaves an allow rule out). With it all six process
    # profiles of Firefox 160 compile (checked on the build host against Mozilla's sources).
    ./patches/0008-libsandbox-profile-language-for-firefox.patch
  ];
})
