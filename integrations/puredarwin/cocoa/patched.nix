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
#   0021  AppKit: faults found with Terminal.app: an undeclared variadic call in nib loading, a nil
#         window title, exceptions from run-loop callouts reported instead of ending the program,
#         nib objects replaced before they were collected, +removeFrameUsingName:, NSViewController
#         without a nib, NSPathControl's delegate; an application's own fonts (ATSApplicationFontsPath)
#         registered for the process; a nib window without a maximum size has none (it was 0 by 0)
#   0022  WindowServer: PD_WS_DUMP_DIR writes the picture a window presents (a diagnostic where the
#         compositor has no screen capture)
#   0023  CoreText: Menlo, Monaco, SF Mono and any family whose name says it is fixed-pitch resolve to
#         an installed fixed-pitch face (a terminal measured its cells from the proportional
#         default); the fixed-pitch UI font comes from the registry; CTFontDrawGlyphsWithAdvances
#         and CTFontShouldAntiAlias (private, imported by Terminal.app)
#   0024  AppKit: +[NSFont fontWithName:size:] asks CoreText for a name no installed face carries;
#         a selection naming rows the content does not have selects nothing (it ended the program);
#         a binding whose key is missing is reported once and does not stop the change that
#         notified it; a font archived without a size has the system font size (labels were dots)
#   0025  CoreGraphics: the context getters for anti-aliasing and font smoothing and the private
#         CGS/CPS window-server calls Terminal.app imports (answers of a Wayland client)
#   0026  Onyx2D: -copyBitsInRect:toPoint:gState: for the bitmap context (-[NSView scrollRect:by:]
#         raised "only defined for abstract class": Terminal.app scrolls its text this way)
#   0027  AppKit: the constraint solves asked for inside one resize cascade are done once, at its end
#         (every view holding constraints had the whole tree solved twice per resized view: 388
#         solves, 4.8 s, while Terminal.app's preferences nib was decoded)
#   0028  AppKit: the application's main menu as a strip, the first row of each titled window that is
#         not a panel, where the compositor draws the frames (the desktop has no global menu bar):
#         the application's own NSMenu in a horizontal NSMenuView, each title's menu an ordinary
#         popup; pointer and keyboard (Control-F2, arrows); counted as frame, not content. Menu rows
#         are measured once and only the rows whose highlight changed are drawn again
#         (0028 also: on such a desktop an application ends when its last window is closed, as it
#         could not be reached again; PDQuitsWithLastWindow=NO keeps it)
#   0029  AppKit: more faults found with Terminal.app: removing the selected NSTabViewItem selected
#         item -1 (closing a window ended the program); -[NSAttributedString URLAtIndex:effectiveRange:];
#         a nib's toolbar kept no items (visibility classes; emptied when its delegate was
#         connected); -[NSWorkspace typeOfFile:error:] answers the type the application declares
#   0030  WindowServer: a window of a program without a bundle identifier gets the program's name as
#         its app id
#   0031  AppKit: the content bindings of a pop-up button (content, contentObjects, contentValues,
#         selectedObject): arrays of objects were taken for titles; the bound selection is applied
#         again when the content arrives after it
#   0032  AppKit: the size of an unbounded one-style string is remembered (a measurement is a pass of
#         the whole text system: 5 ms on the M1, and Terminal.app made 1384 at launch); a title up to
#         an eighth too wide for its place is drawn narrower instead of cut with an ellipsis
#   0033  AppKit: scrollers in the macOS 26 look (no arrow buttons, a rounded bar); PDControlLook=15
#         keeps the old one
#   0034  CoreText: one graphics font per installed face (a fontconfig search and a FreeType face per
#         CTFont: Terminal.app makes a CTFont for each line it draws); a directory registered through
#         CTFontManagerRegisterFontsForURL goes through fontconfig's cache of that directory
#   0035  AppKit: glyph advances from the metrics table (FT_Get_Advance) instead of loading each
#         glyph's outline; an application's fonts registered as their directory (cached by fontconfig,
#         instead of three scans of each file at every launch); the main menu is not archived when
#         there is no menu transport
#   0036  WindowServer: a flush that finds every frame busy is presented when the compositor releases
#         one (it waited up to 100 ms on the main thread and was then dropped); PD_WS_TIMING=1 prints
#         the wall time of each commit and release; kWSWindowTakeToplevel: the tabs of a window share
#         one toplevel (two records exchange their identities)
#   0037  Onyx2D: one font per name (each was a fontconfig match, the whole file read and a FreeType
#         face opened: AppKit makes an NSFont by name for every string it measures)
#   0038  AppKit: more found with Terminal.app: +[NSCountedSet setWithCapacity:] (its check for running
#         processes when it quits)
#   0039  AppKit: window tabs (NSWindowTabGroup, NSWindowTab, addTabbedWindow:ordered:, Show Next/Previous
#         Tab, Move Tab to New Window, Merge All Windows) as one toplevel per group, with a tab bar under
#         the menu strip; structure from Finch's NSWindowTabbing.m (MIT OR Apache-2.0, commit 2d9b1bb)
#   The five frameworks below are compiled with -O2 (upstream's recipes compile without -O: every
#   drawing, text and event path ran unoptimised); -fno-strict-aliasing for old code that casts.
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
    ./patches/0021-appkit-fixes-found-with-terminal.patch
    ./patches/0024-appkit-fonts-bindings-found-with-terminal.patch
    ./patches/0027-appkit-layout-once-per-resize.patch
    ./patches/0028-appkit-window-menu-strip.patch
    ./patches/0029-appkit-fixes-found-with-terminal-2.patch
    ./patches/0031-appkit-popup-button-content-bindings.patch
    ./patches/0032-appkit-measured-sizes-remembered.patch
    ./patches/0033-appkit-scroller-look-macos26.patch
    ./patches/0035-appkit-glyph-advances-font-directory-menu-archive.patch
    ./patches/0038-appkit-found-with-terminal-3.patch
    ./patches/0039-appkit-window-tabs.patch
  ];
  windowserverSrc = patchedDir "src/Frameworks/WindowServer" [
    ./patches/0004-windowserver-server-decorations-and-scale.patch
    ./patches/0018-windowserver-popup-placement.patch
    ./patches/0022-windowserver-presented-frame-dump.patch
    ./patches/0030-windowserver-app-id-without-bundle-identifier.patch
    ./patches/0036-windowserver-present-on-release-tab-toplevel.patch
  ];
  authenticationServicesSrc = patchedDir "src/Frameworks/AuthenticationServices" [
    ./patches/0003-authenticationservices-web-session-browser-side.patch
  ];
  onyx2dSrc = patchedDir "src/Frameworks/Onyx2D" [
    ./patches/0006-onyx2d-glyphs-at-device-scale.patch
    ./patches/0013-onyx2d-png-straight-alpha-byte-order.patch
    ./patches/0026-onyx2d-copy-bits.patch
    ./patches/0037-onyx2d-font-by-name-once.patch
  ];
  # upstream's framework recipes name no optimisation level: their compile lines get one here
  optimised = d: d.overrideAttrs (old: {
    buildPhase = builtins.replaceStrings [ "$cc $cflags -c" ] [ "$cc $cflags -O2 -fno-strict-aliasing -c" ] old.buildPhase;
  });
  onyx2d = optimised (pd."onyx2d-arm64".override { src = onyx2dSrc; });
  coretextSrc = patchedDir "src/Frameworks/CoreText" [
    ./patches/0023-coretext-fixed-pitch-families-private-draw.patch
    ./patches/0034-coretext-graphics-font-cache-font-directories.patch
  ];
  coretext = optimised (pd."coretext-arm64".override { src = coretextSrc; });
  coregraphicsSrc = patchedDir "src/Frameworks/CoreGraphics" [
    ./patches/0025-coregraphics-private-additions.patch
  ];
  coregraphics = optimised (pd."coregraphics-arm64".override { src = coregraphicsSrc; });
  wp = "${pd."wayland-protocols"}/share/wayland-protocols";
  # upstream's recipe names every protocol and object file; the three protocols the patch
  # uses are generated and linked the same way
  windowserver = optimised ((pd."windowserver-arm64".override { src = windowserverSrc; }).overrideAttrs (old: {
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
  }));
  appkit = optimised (pd."appkit-msm-arm64".override { src = appkitSrc; inherit windowserver; });
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
  inherit coretextSrc coretext coregraphicsSrc coregraphics;
}
