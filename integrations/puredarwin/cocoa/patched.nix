# SPDX-License-Identifier: MIT
# Our changes to upstream's frameworks, as patches against the pinned PureDarwin tree
# (./patches, one topic each, paths from the tree's root) and the arm64 builds made from the
# patched sources with upstream's own recipes.
#
#   0001  tip cherry-pick (upstream 71a9d0b0, part): NSFontWeight constants, NSImageSymbolConfiguration
#   0002  AppKit: NSSharingServicePicker
#   0003  AuthenticationServices: the browser side of ASWebAuthenticationSession
#   0004  WindowServer: server-side decorations (xdg-decoration), scale (wp_fractional_scale +
#         wp_viewporter, or the output's whole scale), size limits
#   0005  AppKit: no title bar inside a compositor-drawn frame, backing stores at the scale,
#         zoomed and miniaturised state from the compositor's frame
#   0006  Onyx2D: glyphs rasterised at the device scale (text was point-size bitmaps on a scaled surface)
#   0011  AppKit: the layout API (intrinsic and fitting sizes, alignment rectangles, baselines,
#         compression resistance, -layout/-updateConstraints), intrinsic sizes used by the solver
#   0012  AppKit: control geometry and drawing as macOS 26 has them (measured), the macOS 15 look
#         kept behind the PDControlLook default
#   0013  Onyx2D: the PNG encoder writes straight alpha and reads "default" byte order as the
#         rest of Onyx2D does (pictures of a bitmap context had red and blue exchanged)
#   0015  AppKit: NSPopUpView built its array with +init then +alloc (every pop-up button crashed)
#   0016  AppKit: discarding the events before one event tested that event's own type and removed
#         every earlier event (a tracking loop's periodic timer threw away mouse and key events)
#   0017  AppKit: popups (menus, lists, tool tips) for a compositor that places them: one tracking loop
#         for menus, submenus, pop-up and pull-down buttons and combo boxes, with the keyboard
#   0018  WindowServer: a popup's anchor and kind in its positioner (flip, slide), where the compositor
#         put it reported back, a held button kept across the surfaces of an open menu
#   0019  tip cherry-pick: NSStackView, NSAccessibilityElement (files as at upstream c713d8fb)
#   0020  AppKit: names applications built against the macOS 26 SDK import (first user: Apple's
#         Terminal.app, docs/roadmap/issues/SF-Y-terminal-app.md): constants, accessibility helpers,
#         gesture recognizers, NSTextFinder, private classes that answer nothing to what they lack
#   0009  Security: SecItemCopyMatching writes its result on every path and answers
#         errSecItemNotFound when nothing was found (Firefox crashed on the unwritten result)
#   (0007 and 0008 are libSystem's sandbox profile compiler: ./build-libsystem-cocoa.nix)
#
#   nix-build --extra-experimental-features 'nix-command flakes' patched.nix -A appkit --no-out-link
let
  a = import ./arm64.nix;
  inherit (a) pkgs pd;
  # one framework directory of the tree with the patches that touch it applied
  patchedDir = dir: patches: pkgs.runCommand "puredarwin-${builtins.baseNameOf dir}-src-cocoa" { } ''
    mkdir -p tree/${dir}
    cp -r ${a.src + "/${dir}"}/. tree/${dir}/
    chmod -R u+w tree
    cd tree
    for p in ${toString (map (p: "${p}") patches)}; do
      echo "applying $p"
      patch -p1 --no-backup-if-mismatch < $p
    done
    cp -r ${dir} $out
  '';
  appkitSrc = patchedDir "src/Frameworks/AppKit" [
    ./patches/0001-tip-appkit-font-weights-symbol-configuration.patch
    ./patches/0002-appkit-sharing-service-picker.patch
    ./patches/0005-appkit-server-decorations-and-scale.patch
    ./patches/0011-appkit-layout-api.patch
    ./patches/0012-appkit-controls-macos26.patch
    ./patches/0015-appkit-popup-view-array.patch
    ./patches/0016-appkit-event-queue-discard.patch
    ./patches/0017-appkit-popups.patch
    ./patches/0019-tip-appkit-stackview-accessibilityelement.patch
    ./patches/0020-appkit-macos26-additions.patch
  ];
  windowserverSrc = patchedDir "src/Frameworks/WindowServer" [
    ./patches/0004-windowserver-server-decorations-and-scale.patch
    ./patches/0018-windowserver-popup-placement.patch
  ];
  authenticationServicesSrc = patchedDir "src/Frameworks/AuthenticationServices" [
    ./patches/0003-authenticationservices-web-session-browser-side.patch
  ];
  onyx2dSrc = patchedDir "src/Frameworks/Onyx2D" [
    ./patches/0006-onyx2d-glyphs-at-device-scale.patch
    ./patches/0013-onyx2d-png-straight-alpha-byte-order.patch
  ];
  onyx2d = pd."onyx2d-arm64".override { src = onyx2dSrc; };
  wp = "${pd."wayland-protocols"}/share/wayland-protocols";
  # upstream's recipe names every protocol and object file; the three protocols the patch
  # uses are generated and linked the same way
  windowserver = (pd."windowserver-arm64".override { src = windowserverSrc; }).overrideAttrs (old: {
    buildPhase = builtins.replaceStrings
      [ "cc=\"" "rpc_wayland.o xdg-shell-protocol.o" ]
      [ ''
        for x in unstable/xdg-decoration/xdg-decoration-unstable-v1 staging/fractional-scale/fractional-scale-v1 stable/viewporter/viewporter; do
          n=$(basename $x)
          wayland-scanner client-header ${wp}/$x.xml $n-client-protocol.h
          wayland-scanner private-code ${wp}/$x.xml $n-protocol.c
        done
        cc="''
        "rpc_wayland.o xdg-decoration-unstable-v1-protocol.o fractional-scale-v1-protocol.o viewporter-protocol.o xdg-shell-protocol.o" ]
      (builtins.replaceStrings
        [ "$cc $cflags -c xdg-shell-protocol.c -o xdg-shell-protocol.o" ]
        [ ''
          $cc $cflags -c xdg-shell-protocol.c -o xdg-shell-protocol.o
              for n in xdg-decoration-unstable-v1 fractional-scale-v1 viewporter; do $cc $cflags -c $n-protocol.c -o $n-protocol.o; done'' ]
        old.buildPhase);
  });
  appkit = pd."appkit-msm-arm64".override { src = appkitSrc; inherit windowserver; };
  # Security for arm64 (upstream's build) with SecItemCopyMatching's result defined on every path
  security = pd."security-arm64".overrideAttrs (old: {
    patches = (old.patches or [ ]) ++ [ ./patches/0009-security-secitemcopymatching-defined-result.patch ];
  });
  appFrameworks = a.appFrameworks.override (old: {
    inherit appkit windowserver;
    srcs = old.srcs // { AuthenticationServices = authenticationServicesSrc; };
  });
in {
  inherit appkitSrc windowserverSrc authenticationServicesSrc onyx2dSrc windowserver appkit appFrameworks onyx2d security;
}
