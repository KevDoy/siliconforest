# SPDX-License-Identifier: MIT
# libFoundation for macOS applications (GitHub issue 52, docs/roadmap/issues/SF-Y-terminal-app.md):
# upstream PureDarwin's Foundation for arm64 at the revision of ../rebase/pd.nix (the build the
# disk root has as /usr/lib/libFoundation.dylib) with ./patches/foundation-macos-applications.patch:
#   NSKeyedUnarchiver reads archives made by Apple's NSKeyedArchiver ($archiver/$top/$objects with
#     UID references: compiled nibs, archived fonts and colours); -replaceObject:withObject:
#   a message nothing answers raises NSInvalidArgumentException (objc4's own method ends the
#     process); the forwarding trampoline has unwind information, so the exception reaches handlers
#   key-value coding: collection operators (@count ...), -mutableArrayValueForKey:
#   -[NSAttributedString init], -[NSString cStringUsingEncoding:] that converts, and the
#     methods Terminal.app sends that were missing (strings, index sets, URLs, coders ...)
#   NSUnrecognizedSelectorBacktrace=1 / NSExceptionRaiseBacktrace=1: where a failure comes from
# A file every Objective-C program loads: QEMU pass and the guarded procedure before it replaces
# the disk root's; until then it is tried from a scratch prefix (DYLD_LIBRARY_PATH).
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-foundation.nix
let
  pd = (import ../rebase/pd.nix).pd;
in pd.packages.x86_64-linux."foundation-arm64".overrideAttrs (old: {
  pname = "siliconforest-foundation-macos-apps";
  patches = (old.patches or []) ++ [ ./patches/foundation-macos-applications.patch ];
  patchFlags = [ "-p4" ];
})
