# SPDX-License-Identifier: MIT
# libSystem and dyld for macOS applications built for arm64e against the macOS 26 SDK
# (GitHub issue 52, docs/roadmap/issues/SF-Y-terminal-app.md): the lab's default libSystem
# recipe (whose output carries /usr/lib/dyld and /usr/lib/libSystem.B.dylib) with
#   ../libsystem-default/patches/dyld-arm64e-keys-off.patch
#       dyld measures that pointer signing does nothing in the process it starts (this kernel
#       keeps the PAC keys off and lowers PAC instructions in every executable page) and then
#       loads arm64 libraries into an arm64e program, and arm64e slices into any program.
#   ../libsystem-default/patches/dyld-objc-writes-data-const.patch
#       (applies on top of the first) current linkers put Objective-C reference sections into the
#       read-only __DATA_CONST segment; dyld opens that segment for the runtime while it is told
#       an image is mapped, as Apple's current dyld does, and closes it again.
#   ../libsystem-default/patches/macos26-sdk-imports.patch
#       malloc_type_*, _os_feature_enabled_impl, the qtn_proc calls, os_variant_is_basesystem,
#       strnvis.
# Until these two are in ../libsystem-default/build-libsystem.nix's own list (a replacement of
# files every program loads: QEMU pass and the guarded procedure first), this is a recipe of
# its own.
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-libsystem.nix
(import ../libsystem-default/build-libsystem.nix).overrideAttrs (old: {
  pname = "siliconforest-libsystem-macos-apps";
  patches = (old.patches or []) ++ [
    ../libsystem-default/patches/dyld-arm64e-keys-off.patch
    ../libsystem-default/patches/dyld-objc-writes-data-const.patch
    ../libsystem-default/patches/macos26-sdk-imports.patch
  ];
})
