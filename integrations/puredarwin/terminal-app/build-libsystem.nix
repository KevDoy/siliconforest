# SPDX-License-Identifier: MIT
# libSystem and dyld for macOS applications built for arm64e against the macOS 26 SDK
# (GitHub issue 52, docs/roadmap/issues/SF-Y-terminal-app.md): since the replacement on the M1
# this is the lab's default libSystem recipe itself, whose patch list has
#   ../libsystem-default/patches/dyld-arm64e-keys-off.patch
#   ../libsystem-default/patches/dyld-objc-writes-data-const.patch
#   ../libsystem-default/patches/macos26-sdk-imports.patch
# (the recipe says what each does). Kept as a name the recipes of this directory import.
#
# SF_LIBSYSTEM=opt: the same source and patches compiled with optimisation
# (../libsystem-default/build-libsystem-opt.nix; SF-Y, "Performance: optimised core libraries").
# An environment variable (as ../rebase/pd.nix's SF_PD) so that the tree other agents build stays
# the installed one until the optimised build has been rehearsed and installed.
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-libsystem.nix
let which = builtins.getEnv "SF_LIBSYSTEM"; in
if which == "opt" then import ../libsystem-default/build-libsystem-opt.nix { }
else if which == "" || which == "debug" then import ../libsystem-default/build-libsystem.nix
else throw "SF_LIBSYSTEM=${which}: expected opt or debug"
