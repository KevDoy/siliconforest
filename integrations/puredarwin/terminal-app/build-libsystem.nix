# SPDX-License-Identifier: MIT
# libSystem and dyld for macOS applications built for arm64e against the macOS 26 SDK
# (GitHub issue 52, docs/roadmap/issues/SF-Y-terminal-app.md): since the replacement on the M1
# this is the lab's default libSystem recipe itself, whose patch list has
#   ../libsystem-default/patches/dyld-arm64e-keys-off.patch
#   ../libsystem-default/patches/dyld-objc-writes-data-const.patch
#   ../libsystem-default/patches/macos26-sdk-imports.patch
# (the recipe says what each does). Kept as a name the recipes of this directory import.
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-libsystem.nix
import ../libsystem-default/build-libsystem.nix
