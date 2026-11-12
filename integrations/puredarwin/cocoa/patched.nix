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
  ];
  windowserverSrc = patchedDir "src/Frameworks/WindowServer" [
    ./patches/0004-windowserver-server-decorations-and-scale.patch
  ];
  authenticationServicesSrc = patchedDir "src/Frameworks/AuthenticationServices" [
    ./patches/0003-authenticationservices-web-session-browser-side.patch
  ];
  onyx2dSrc = patchedDir "src/Frameworks/Onyx2D" [
    ./patches/0006-onyx2d-glyphs-at-device-scale.patch
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
  appFrameworks = a.appFrameworks.override (old: {
    inherit appkit windowserver;
    srcs = old.srcs // { AuthenticationServices = authenticationServicesSrc; };
  });
in {
  inherit appkitSrc windowserverSrc authenticationServicesSrc onyx2dSrc windowserver appkit appFrameworks onyx2d;
}
