# SPDX-License-Identifier: MIT
# The desktop root (./build-desktop-root.nix: our patched WindowServer, AppKit and Onyx2D) plus
# what the official macOS Firefox links beyond it: upstream's small application frameworks
# built for arm64 with our AuthenticationServices patch (./patched.nix), Security and
# SystemConfiguration (upstream's arm64 builds), and our own Metal, MetalKit and Accelerate
# (./build-own-frameworks.nix).
let
  a = import ./arm64.nix;
  p = import ./patched.nix;
in import ./build-cocoa-root.nix {
  extra = [
    p.appFrameworks
    a.pd."security-arm64"
    a.pd."systemConfiguration-arm64"
    (import ./build-own-frameworks.nix)
    p.onyx2d p.windowserver p.appkit
    (import ./build-hello.nix)
  ];
}
