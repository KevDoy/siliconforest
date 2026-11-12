# SPDX-License-Identifier: MIT
# The Cocoa system root for the SiliconForest desktop: ./build-cocoa-root.nix with our patched
# WindowServer and AppKit (./patched.nix: compositor-drawn frames, display scale, glyphs at the device scale) laid over
# upstream's, and the test application.
let
  p = import ./patched.nix;
in import ./build-cocoa-root.nix {
  extra = [ p.onyx2d p.windowserver p.appkit (import ./build-hello.nix) ];
}
