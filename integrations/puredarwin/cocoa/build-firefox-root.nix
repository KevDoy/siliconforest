# SPDX-License-Identifier: MIT
# The Cocoa system root (./build-cocoa-root.nix) plus what the official macOS Firefox links
# beyond it: upstream's small application frameworks built for arm64 (./arm64.nix), Security
# and SystemConfiguration (upstream's arm64 builds), and our test application.
let
  a = import ./arm64.nix;
in import ./build-cocoa-root.nix {
  extra = [
    a.appFrameworks
    a.pd."security-arm64"
    a.pd."systemConfiguration-arm64"
    (import ./build-hello.nix)
  ];
}
