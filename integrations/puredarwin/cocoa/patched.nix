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
#         CTFontManagerRegisterFontsForURL goes through fontconfig's cache of that directory; a named
#         instance of a variable font is drawn as that instance (its Bold was drawn as Regular)
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
#         processes when it quits), -[NSTextField setLineBreakMode:], a family's plain Bold and Italic
#         for -convertFont:toHaveTrait: (not the first face with the trait: Heavy, Light Italic)
#   0039  AppKit: window tabs (NSWindowTabGroup, NSWindowTab, addTabbedWindow:ordered:, Show Next/Previous
#         Tab, Move Tab to New Window, Merge All Windows) as one toplevel per group, with a tab bar under
#         the menu strip; structure from Finch's NSWindowTabbing.m (MIT OR Apache-2.0, commit 2d9b1bb)
#   0040  AppKit: -getRectsBeingDrawn: answers the rect that is drawn (Onyx2D clips to one rectangle, so
#         the views behind fill all of it: Terminal filled only the dirty rows and the rows between kept
#         the window's grey); dirty rects far apart are drawn one at a time; dirty rects are whole
#         device pixels (at 150% a rect of whole points left a seam of the grey at each row edge)
#   0041  AppKit: a window starts opaque, as on macOS (Terminal turns a window with a translucent profile
#         clear only when -setOpaque:NO changes that), and its background is copied, not drawn over
#         what the backing store held; a window without NSTitledWindowMask (a sheet, an alert) has no
#         title bar and no room for one, a sheet a hairline border, and the WindowServer is told a
#         sheet's parent; the offscreen window of an image's cache does not take the current context
#         while a view draws (Terminal's profile list drew into it after the first thumbnail)
#   0042  Onyx2D: PNG entries of an .icns (ic07 to ic14, all a current application icon has), upstream's
#         change at the tip (29bc3a57 "Onyx2D: PNG icons in ICNS"): Terminal's icon read as nothing
#   0043  WindowServer: a window's parent (kWSWindowSetParent, xdg_toplevel.set_parent): the compositor
#         keeps a sheet over its parent and centres it there
#   0044  AppKit: alerts laid out as on macOS 26 (metrics after Finch's NSAlert.m, MIT OR Apache-2.0):
#         420 points wide, the application's icon, the message bold and wrapped, buttons sized to their
#         titles; NSRunAlertPanel and NSBeginAlertSheet are NSAlert (they were a panel that put the
#         message in its title bar); Return, Escape and Command-D buttons; the application's icon is
#         its bundle's from the start and is what NSApplicationIcon names
#   0045  AppKit: a space that opens a layout fragment is a place to wrap (lines broke inside words
#         and began with a space after a kerning pair had ended a fragment)
#   0046  AppKit: the NIBArchive reader's -decodeFloatForKey: answers a float as NSCoder declares it (it
#         answered a double: every caller read 0, so a nib's column widths and row heights were 0);
#         labels a little too wide for a nib laid out for macOS's system font are drawn narrower when
#         they clip too, and their descenders are not cut; a shortcut typed with Shift shows as macOS
#         shows it (Shift-Command-[ for Command-{); a menu's shadow layers are rings round the panel,
#         not eight fills of its whole area
#   0047  AppKit: a control's bound value goes to the model when it sends its action (a click changes
#         the cell, which no key-value observation of the control hears), and a controller's
#         selection proxy sets a value on each selected object: Terminal's preference checkboxes now
#         change the profile and persist
#   0048  AppKit: NSTextFinder with a find bar (search field, previous/next, Done, replace row), taken
#         from Finch's NSTextFinder.m (MIT OR Apache-2.0, notice in the file): Terminal's Command-F,
#         Find Next/Previous, Use Selection for Find (the finder before had no bar and beeped); the bar
#         takes the find actions while its field has the keys; a Command key that no menu took inserts
#         no text (Command-G typed a "g")
#   0049  AppKit: a font name's PostScript name (a fontconfig listing of every installed face) and the
#         fallback font for a character are remembered: a menu opened in about 390 ms once the CJK fonts
#         were installed, now about 195 ms
#   0050  AppKit: tab views as on macOS 26 (rounded segments, the selected one white, a faint rounded
#         pane with a hairline) instead of raised grey bevels, on which a disabled label vanished
#   0051  AppKit: drawn once: a menu that opens and a tab that is switched to were drawn twice; the
#         window's menu strip laid out its titles again whenever the content under it was drawn (at
#         150% the content's top edge falls inside a pixel row of the strip); the strip stays in full
#         screen; a tab bar redraws when a tab's title changes; an empty Services menu shows a
#         disabled "No Services Apply" as on macOS (it opened as nothing)
#   0052  AppKit: full screen through the compositor (xdg_toplevel set/unset_fullscreen, the window's
#         state follows the compositor's, will/did notifications and delegate calls, the menu item
#         says Enter or Exit Full Screen); Command-= is Command-+ (Bigger); a key equivalent validates
#         the item it matches, not every menu; a title set again unchanged is no message and no redraw;
#         a configure that a later one overtook is not applied (a drag resized the window once per
#         pointer step, each a new backing store and a whole drawing, and fell behind)
#   0053  CoreText: the line made for an attributed string is kept for the same text and attributes
#         (a terminal lays out every row of every frame); the CTFont made for an NSFont is kept
#   0055  AppKit: the save and open panels drawn by AppKit (name, folder, the folder's items, the
#         application's accessory view, Cancel and Save/Open), macOS's API for them
#         (-beginSheetModalForWindow:completionHandler:, -setNameFieldStringValue:, -directoryURL ...):
#         they were left to the display, which had none (Export Text As, Export Settings, Open)
#   0056  AppKit: printing saves a PDF (there are no printers: the print panel says so and offers
#         Save as PDF, the operation draws its pages into a PDF context);
#         -runOperationModalForWindow:delegate:didRunSelector:contextInfo:; a help book that is asked
#         for says that help is not available (the alert of -showHelp:) instead of raising
#   0057  AppKit: -getRectsBeingDrawn: and -needsToDrawRect: asked outside a display (a view drawn
#         into an image by calling -drawRect:, Terminal's profile thumbnails) answer the whole view
#   0058  WindowServer: a window AppKit brings to the front (kWSWindowOrder: Command-`, the Window
#         menu) is raised and focused by the compositor through xdg_activation_v1; PD_WS_TIMING
#         also logs each configure (the time from a configure to the frame of its size)
#   0059  AppKit: found walking Terminal's menus: a table column can be hidden (-isHidden,
#         -setHidden:; Terminal's inspector); a cell keeps an object value that cannot be copied
#         (its process list); the font bindings of a cell (fontBold, fontItalic, fontName, fontSize);
#         a foreground "colour" that is not one draws black instead of raising in every redraw;
#         a binding's value transformer named by a class nobody registered is an instance of that
#         class, and a table column's bindings go through it (the inspector's process names and
#         colours); an empty key equivalent of a button matches nothing (Shift alone opened the
#         inspector's pull-down)
#   0054  Onyx2D: a filled rectangle, a straight stroke along an axis and a stroked outline whose
#         sides are straight (a rectangle, a rounded rectangle: its arcs alone go through the edge
#         rasterizer) are covered directly, a row at a time; the rasterizer's winding buffer (2 MB at
#         the screen's width) is kept for the next context instead of a fresh calloc per context
#         (a window gets one at every size)
#   0100..0106: Apple's Activity Monitor (docs/roadmap/issues/SF-AA-activity-monitor.md, GitHub issue 53;
#   regenerated by ./patches/mkpatches-0100.sh)
#   0100  AppKit: names Activity Monitor imports (NSSearchToolbarItem, NSWorkspaceDidWakeNotification, the
#         compositional layout's description objects kept as data, +[NSColor colorNamed:], ...); the search
#         field sends its action while typing
#   0101  AppKit: view-based NSTableView/NSOutlineView (cell views, row views, the archived cell-view nibs,
#         reuse), hidden table columns, the header's sort indicator; a header click asks the delegate
#         -tableView:shouldSelectTableColumn: (a row click no longer does); a table row view is the top of its
#         own layouts
#   0102  CoreGraphics: CGSEventIsAppUnresponsive, CGSRegisterNotifyProc
#   0103  AppKit: without EGL a layer-backed view is drawn as an ordinary view (no layer context through CGL)
#   0104  AppKit: NIBArchive booleans the right way round (value type 4 is false, 5 true: every BOOL a nib
#         carried was read inverted), a cell's three line-break bits, custom views' constraints and
#         identifier, a constrained label re-measured when its string's length changes, bindings apply a
#         change made off the main thread on the main thread, the menu strip on full-size-content windows
#   0105  AppKit: an application's named colours from its Assets.car (Finch's catalog reader, MIT OR Apache-2.0)
#   0106  AppKit: what the corrected booleans handed to Auto Layout (Terminal's Preferences and Inspector panes
#         overlapped, a label cut at the left edge): a nib's standard-space constraints (NSSymbolicConstant
#         NSSpace: 20 points to the superview's edge, 8 between siblings; they were 0), the content hugging and
#         compression resistance priorities a nib archives, NSMatrix's intrinsic size from its cells (it was
#         10000 by 10000); a table header as wide as its clip view where no vertical scroller takes room (the
#         last column's title was under an empty corner); PD_LAYOUT_TRACE=1 prints each layout's constraints
#         and frames
#   0107  AppKit: what Activity Monitor showed wrongly or slowly. Our own drawings for the few system symbol
#         images asked for (info.circle, ellipsis.circle, xmark.octagon, magnifyingglass, arrow.forward.circle.fill: SF Symbols are
#         Apple's font and not copied); a toolbar item's view centred in its item, the toolbar with a margin
#         above and below (an icon-only toolbar's views were pushed 4 points up and cut); a hidden menu takes no
#         title in the window's menu strip; an image cell's image is its object value (an image set to nil
#         showed the nib's placeholder, the application's icon, on every process); a table keeps its columns'
#         widths unless its column autoresizing style says otherwise, and its header draws an empty header
#         right of the last column; a view-based table reloads the rows shown in place (the cell's own view is
#         given back to it) and draws only what changed; a dirty view scrolled out of sight is not drawn; a
#         system catalog colour is resolved once per appearance; PD_CA_TRACE is read once (it was a getenv per
#         -setFrame:); a pop-up (not pull-down) menu with items has one selected (Activity Monitor's Send Signal
#         sent signal 0). Activity Monitor's own CPU: docs/roadmap/issues/SF-AA-activity-monitor.md, Performance
#   0108  AppKit: NSMenuItem's object getters (title, attributed title, key equivalent, image, represented
#         object) answer retained and autoreleased, as macOS's atomic properties do. With libFoundation's
#         CFAutorelease fix (terminal-app/build-foundation.nix, 14 November evening) strings are freed when
#         their pool ends; Terminal keeps a removed item's key equivalent (macOS's one-letter strings are
#         tagged pointers, never freed) and ended in -[NSMenuItem initWithTitle:...] without this
#   0109  CoreGraphics (ImageIO re-exports it): CGImageSourceCreateThumbnailAtIndex (oriented, scaled to
#         kCGImageSourceThumbnailMaxPixelSize) and image metadata (tags by path, made, copied, set, read,
#         removed; a source's EXIF and TIFF properties as exif:/tiff: tags), for Finch's Contacts and
#         MediaAccessibility (runs/m1-coord/requests-for-CLAUDE.md)
#   0110  AppKit: the modern NSCollectionView from Finch (MIT OR Apache-2.0; data source, registered items, flow
#         and grid layouts, selection, the older content/prototype form; NSCollectionView.m, NSCollectionViewLayout.m,
#         notices kept): an item with nothing registered comes from a nib or class of the identifier's name, as
#         on macOS; NSViewController -isViewLoaded; PD_CV_TRACE=1 prints each layout pass. Activity Monitor's
#         Window > CPU Usage and CPU History open (they ended the application or did not open). Needs
#         libFoundation's NSIndexPath: AppKit is built against ../terminal-app/build-foundation.nix (below)
#   0111  AppKit: images of an application's asset catalog for +[NSImage imageNamed:] (Finch's CoreUI bitmap,
#         deepmap2 and LZFSE/LZVN/zip decoding and rendition choice; libcompression and zlib looked up at run
#         time) and Finch's symbol glyphs (about 150 names drawn in code, with .fill/.circle/.slash/.badge) after
#         our own five (NSSymbolImages.m)
#   0112  AppKit: PD_SOLVE_TIME=1 prints each whole-tree constraint solve's time and the sum (Finch's solver core
#         in place of ours was measured with it and taken back: Terminal's Preferences 1574 ms of solving with
#         ours, over 10000 ms with Finch's used as a throwaway solver; SF-AA, "Taken from Finch")
#   0113  AppKit: macOS 26's toolbar buttons: an image button or pull-down that is a toolbar item's view draws its
#         glyph alone, centred, one height (28 points) for all, a row of them in one rounded capsule, a small
#         down chevron after a pull-down's glyph, the room around each glyph the same; a preferences toolbar's
#         selected item in a soft rounded rectangle (Activity Monitor's stop/info/actions row, owner's photo)
#   0114  AppKit: macOS's overlay scrollers (the style of a Mac with a trackpad; legacy with AppleShowScrollBars=Always
#         or PD_SCROLLERS=legacy): a thin rounded knob over the content, no track or arrows, wider on a faint
#         track while dragged, shown when the user scrolls (-flashScrollers) and faded a second later (a one-shot
#         timer, then a fade that stops itself: nothing runs at rest); no room taken, no corner, a scroller
#         with nothing to scroll hidden; in the legacy style the square where two scrollers meet is painted
#   0115  AppKit: -[NSEvent CGEvent] autoreleases with -autorelease (CoreFoundation's CFAutorelease does not
#         autorelease a CF-owned object: every CGEvent asked for leaked)
#   0116  AppKit (COCOA4, the owner's findings): a window kept after -close keeps its window-server window
#         (destroyed with the object), so Terminal's Preferences opens again after Command-W; NSCursor's +hide/
#         +unhide counted and +setHiddenUntilMouseMoves: hides only until the pointer moves (the pointer stayed
#         invisible over Terminal after typing); an application outlives its last window (PDQuitsWithLastWindow
#         =YES for the old rule); NSPathCell without -[NSURL initWithScheme:host:path:]; +[NSColor
#         colorWithRed:green:blue:alpha:] (Activity Monitor's CPU windows raised on it); the window server
#         thread drains a pool per message (every NSEvent it made stayed); NSColorPanel built in code when its
#         nib is missing (colour swatch, palette, red/green/blue sliders and fields, opacity slider and field shown
#         unless +[NSColor ignoresAlpha]): Terminal's colour wells opened nothing; the HSB conversions in CGFloat
#         (they wrote floats into doubles: -getHue:... answered denormals and +colorWithCalibratedHue:... stack
#         garbage: Terminal's marks pure blue, a red line around its window); the desk's per-application menu
#         socket ($XDG_RUNTIME_DIR/app-menu/<pid>: the main menu, invoke, the menu strip on/off); Terminal's
#         Window Groups symbol (rectangle.3.offgrid, our drawing)
#   0117  WindowServer: a pointer hidden until it moves is shown at its next motion or entry, on the input
#         thread; PD_CURSOR_TRACE=1
#   0118  CoreGraphics: CGColorSpaceCreateWithICCData/ICCProfile/ICCBased keep the profile (CGColorSpaceCopyICCData),
#         model and components from its header; CGColorSpaceCreateIndexed and the indexed getters (Codex's
#         bodies); kCGColorSpaceCoreMedia709
#   0119  Onyx2D: the path a gradient fills under a clip path is released (its edges leaked at each one)
#   0120  AppKit (TERM5, the owner's hands-on round): -centerScanRect: and -backingAlignedRect:options: (NSView,
#         NSWindow) put edges on the backing store's own pixels (its pixels per point along each axis), dirty
#         rects too (Terminal's rows and selection no longer end half way through a pixel at 150 %); with no
#         preference set, transparency is reduced while the compositor cannot blur behind a window (Terminal's
#         default profile then draws opaque, as macOS with Reduce Transparency; `defaults write
#         com.apple.universalaccess reduceTransparency -bool false` gives translucency back); windows tell the
#         window server whether they are opaque; menus: motion coalesced, a submenu opens after the pointer
#         rests 60 ms on its item instead of being built while the pointer passes, PD_WS_TIMING lines;
#         NSPathControl has no intrinsic width (its cell's 10000 points made Terminal's Profiles > Text box
#         10000 points wide: the black bar over "Background"); PD_IMAGE_TRACE
#         (night, later): the colour panel's accessory view (Terminal's background well), one toggle per click of
#         a colour well; a wrapping label longer than its width is as tall as its lines; click sequences need
#         the same window, 4 points and 0.5 s (com.apple.mouse.doubleClickThreshold), a table's double action
#         both clicks on one row; a window ordered out gives up key and main (Activity Monitor ended with
#         SIGSEGV when its inspector closed); tab accessory views in a slot of their own; one-line strings
#         truncated head/middle/tail with an ellipsis; the help button's question mark; crash reports in
#         ~/Library/Logs/DiagnosticReports/<name>-<pid>.crash (PD_NO_CRASH_REPORT=1: none)
#         a view taken out of the hierarchy takes the constraints above that refer to it (Activity Monitor's
#         tab switch messaged a freed view); a colour well without a border activates on a click
#   0121  WindowServer: the opaque region of an opaque toplevel; _windowServerCanBlurBehind; PD_WS_SCALE (a test
#         instrument: one application's backing stores at a chosen scale); pointer motion in PD_WS_TIMING
#   0122  Onyx2D: a clip's edges within 1/1024 of a pixel boundary are on it (a dirty rect that went to points
#         and back took one more row, which the window's clear made transparent and no fill covered);
#         O2_FILL_TRACE lists the rectangles filled
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
    ./patches/0040-appkit-dirty-rects-as-drawn.patch
    ./patches/0041-appkit-windows-opaque-untitled-sheets.patch
    ./patches/0044-appkit-alerts-macos-layout.patch
    ./patches/0045-appkit-word-wrap-at-fragment-start.patch
    ./patches/0046-appkit-nib-floats-labels-menu-glyphs.patch
    ./patches/0047-appkit-bindings-write-back.patch
    ./patches/0048-appkit-find-bar.patch
    ./patches/0049-appkit-font-lookups-remembered.patch
    ./patches/0050-appkit-tab-view-macos26.patch
    ./patches/0051-appkit-draw-once.patch
    ./patches/0052-appkit-full-screen-key-equivalents.patch
    ./patches/0055-appkit-save-open-panels.patch
    ./patches/0056-appkit-print-to-pdf-help.patch
    ./patches/0057-appkit-rects-outside-display.patch
    ./patches/0059-appkit-found-walking-the-menus.patch
    ./patches/0100-appkit-activity-monitor-names.patch
    ./patches/0101-appkit-view-based-tables-hidden-columns.patch
    ./patches/0103-appkit-layer-backed-views-without-egl.patch
    ./patches/0104-appkit-found-with-activity-monitor.patch
    ./patches/0105-appkit-asset-catalog-colors-from-finch.patch
    ./patches/0106-appkit-nib-layout-standard-space-priorities.patch
    ./patches/0107-appkit-activity-monitor-look-and-cost.patch
    ./patches/0108-appkit-menu-item-getters-autoreleased.patch
    ./patches/0110-appkit-collection-view-from-finch.patch
    ./patches/0111-appkit-catalog-images-symbols-from-finch.patch
    ./patches/0112-appkit-solve-time-trace.patch
    ./patches/0113-appkit-toolbar-buttons-macos26.patch
    ./patches/0114-appkit-overlay-scrollers.patch
    ./patches/0115-appkit-cgevent-autoreleased.patch
    ./patches/0116-appkit-owner-findings-windows-cursor-colors.patch
    ./patches/0120-appkit-device-pixels-opaque-default-menu-tracking.patch
  ];
  windowserverSrc = patchedDir "src/Frameworks/WindowServer" [
    ./patches/0004-windowserver-server-decorations-and-scale.patch
    ./patches/0018-windowserver-popup-placement.patch
    ./patches/0022-windowserver-presented-frame-dump.patch
    ./patches/0030-windowserver-app-id-without-bundle-identifier.patch
    ./patches/0036-windowserver-present-on-release-tab-toplevel.patch
    ./patches/0043-windowserver-sheet-parent.patch
    ./patches/0058-windowserver-activation-configure-timing.patch
    ./patches/0117-windowserver-cursor-shown-when-the-pointer-moves.patch
    ./patches/0121-windowserver-opaque-region-blur-query-test-scale.patch
  ];
  authenticationServicesSrc = patchedDir "src/Frameworks/AuthenticationServices" [
    ./patches/0003-authenticationservices-web-session-browser-side.patch
  ];
  onyx2dSrc = patchedDir "src/Frameworks/Onyx2D" [
    ./patches/0006-onyx2d-glyphs-at-device-scale.patch
    ./patches/0013-onyx2d-png-straight-alpha-byte-order.patch
    ./patches/0026-onyx2d-copy-bits.patch
    ./patches/0037-onyx2d-font-by-name-once.patch
    ./patches/0042-tip-onyx2d-icns-png-entries.patch
    ./patches/0054-onyx2d-rectangles-without-edges.patch
    ./patches/0119-onyx2d-shading-path-released.patch
    ./patches/0122-onyx2d-clip-edges-on-pixels.patch
  ];
  # upstream's framework recipes name no optimisation level: their compile lines get one here
  optimised = d: d.overrideAttrs (old: {
    buildPhase = builtins.replaceStrings [ "$cc $cflags -c" ] [ "$cc $cflags -O2 -fno-strict-aliasing -c" ] old.buildPhase;
  });
  onyx2d = optimised (pd."onyx2d-arm64".override { src = onyx2dSrc; });
  coretextSrc = patchedDir "src/Frameworks/CoreText" [
    ./patches/0023-coretext-fixed-pitch-families-private-draw.patch
    ./patches/0034-coretext-graphics-font-cache-font-directories.patch
    ./patches/0053-coretext-lines-and-fonts-remembered.patch
  ];
  coretext = optimised (pd."coretext-arm64".override { src = coretextSrc; });
  coregraphicsSrc = patchedDir "src/Frameworks/CoreGraphics" [
    ./patches/0025-coregraphics-private-additions.patch
    ./patches/0102-coregraphics-activity-monitor-cgs.patch
    ./patches/0109-coregraphics-imageio-thumbnail-metadata.patch
    ./patches/0118-coregraphics-icc-and-indexed-color-spaces.patch
  ];
  coregraphics = optimised (pd."coregraphics-arm64".override { src = coregraphicsSrc; });
  wp = "${pd."wayland-protocols"}/share/wayland-protocols";
  # upstream's recipe names every protocol and object file; the three protocols the patch
  # uses are generated and linked the same way
  windowserver = optimised ((pd."windowserver-arm64".override { src = windowserverSrc; }).overrideAttrs (old: {
    buildPhase = builtins.replaceStrings
      [ "cc=\"" "rpc_wayland.o xdg-shell-protocol.o" ]
      [ ''
        for x in unstable/xdg-decoration/xdg-decoration-unstable-v1 staging/fractional-scale/fractional-scale-v1 stable/viewporter/viewporter staging/xdg-activation/xdg-activation-v1; do
          n=$(basename $x)
          wayland-scanner client-header ${wp}/$x.xml $n-client-protocol.h
          wayland-scanner private-code ${wp}/$x.xml $n-protocol.c
        done
        cc="''
        "rpc_wayland.o xdg-decoration-unstable-v1-protocol.o fractional-scale-v1-protocol.o viewporter-protocol.o xdg-activation-v1-protocol.o xdg-shell-protocol.o" ]
      (builtins.replaceStrings
        [ "$cc $cflags -c xdg-shell-protocol.c -o xdg-shell-protocol.o" ]
        [ ''
          $cc $cflags -c xdg-shell-protocol.c -o xdg-shell-protocol.o
              for n in xdg-decoration-unstable-v1 fractional-scale-v1 viewporter xdg-activation-v1; do $cc $cflags -c $n-protocol.c -o $n-protocol.o; done'' ]
        old.buildPhase);
  }));
  # (0110) against the libFoundation the root has (NSIndexPath is in it), not upstream's
  foundation = import ../terminal-app/build-foundation.nix;
  appkit = optimised (pd."appkit-msm-arm64".override { src = appkitSrc; inherit windowserver foundation; });
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
