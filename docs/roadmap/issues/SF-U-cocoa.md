# SF-U: Cocoa stack (Mac GUI applications on the SiliconForest desktop)

## Goal
Mac GUI applications run on the SiliconForest desktop (Chonkstep) using PureDarwin's open
reimplementation of Apple's application frameworks: first a trivial AppKit application, then
upstream's own applications, then the official macOS Firefox. Owner's requirements for Mac
windows on our desktop (11 November): our own window frame and controls, in every theme
("not to be forced into using the AppKit window controls"), and rendering at the desktop's
scale. Work is done in QEMU on the workstation (arm64 `virt`, TCG; the QEMU stream's
environment); the M1 proof comes later.

Tools and recipes: `integrations/puredarwin/cocoa/`. Run output: `runs/cocoa/` (not committed).
Compositor changes: branch `wm-darwin-cocoa` of `sources/chonkstep-darwin` (worktree
`runs/cocoa/chonkstep-worktree`, local only).

## Upstream state (checked 11 November)
- The development branch of github.com/PureDarwin/PureDarwin is `upstream-xnu-12377` (there is
  no `next`); our pin `e7aa00a6` (6 Nov) is on it, the tip was `c713d8fb` (11 Nov), 119 commits
  later. Fresh clone: `sources/puredarwin-upstream-next`.
- No Firefox image, recipe or download exists in the tree. What exists is the framework work
  "for the official Firefox build" (19 Oct, in our pin), tests (`tests/appkit-ff`,
  `tests/frameworks-ff`, `tests/cg-ff`; at the tip also `tests/ffprobe`), and the website's
  statement that "a handful of unmodified macOS apps run as well, Firefox among them" on the
  x86_64 Argon desktop (`neuswc` compositor).
- The WindowServer is a library inside every application and a plain Wayland client. It
  needs wl_compositor, xdg_wm_base and wl_shm; everything else is optional (layer-shell for
  desktop-level windows, `pd_window_position_v1` for absolute positions, `pd_surface_host_v1`
  for layers hosted across processes, subsurfaces, dmabuf, data device, output management).
  Every NSWindow is an xdg_toplevel. Upstream has no display scaling at all (backing scale is
  the constant 1 at the pin and at the tip) and always draws its own title bar.
- The pin already has arm64 builds of the whole stack in its flake (`appkit-msm-arm64`,
  `terminal-msm-arm64`, `calculator-msm-arm64`, `argon-desktop-msm-arm64`, ...), made for a
  Qualcomm board. The tree generates its own SDK; no Apple SDK is needed.
- Decision: no rebase of the lab for this stream. Tip commits are taken as separate patches
  (`patches/0001-tip-*`), so they drop out if the lab is rebased.

## Upstream's own result, reproduced as far as it went (x86_64, KVM)
Run by a sub-agent under `runs/cocoa/upstream-x86/` until it was cut off; the facts below were
read from its files and its still-running guest.
- Built at the pin: upstream's x86_64 Argon desktop image with Firefox added
  (`scripts/ff-image.nix`, images `img/result-argon`, `img/result-argon-ff`). It boots under KVM
  to the Argon desktop: `run5/shots/ff158-final-by-parent.png` (menu bar, Dock, Terminal).
  Earlier boots failed in three ways before one worked (`logs/run1..4-*.log`: a spinlock
  panic, a hang with 2 CPUs, an xHCI spin).
- Firefox 160.0 (current official) on that image: refused by dyld.
  `Symbol not found: _ASWebAuthenticationSessionErrorDomain ... Expected in: AuthenticationServices`
  (guest `/var/log/ff-a2.log`). The same symbol is missing at upstream's tip.
- Firefox 158.0 (the version upstream's tests name): XUL loads and runs. Seen: a sandbox
  profile that upstream's libsandbox cannot parse (`sandbox_init() failed with error "line 10:
  define expects a name and a value"`), `NSApplication: published 1 menu items for 2 windows
  to the menu bar`, kernel `EXC_GUARD` reports for the firefox process, two child processes
  dying, then Mozilla's crash reporter starting and panicking on `Class with name NSStackView
  could not be found`. No Firefox window was on the screen. NSStackView and sandbox-profile
  work are in upstream's tip (10 and 11 November), so the claim on the website is presumably
  true of the tip, not of our pin. The tip image was not built: it needs 346 derivations here
  (toolchain, LLVM, kernel, Wine: everything), against 171 for the pin.
- Not done: the tip, Firefox with the sandbox switched off, any page render.

## Design: Mac applications as first-class windows
A Mac application is an ordinary Wayland client of Chonkstep (the WindowServer library inside
it). The standard protocols carry everything; nothing of upstream's desktop (`neuswc`'s
private protocols, layer-shell, MenuBar.app) is needed, and the compositor needed one small
addition. The alternative, teaching Chonkstep upstream's `pd_*` protocols and layer-shell, would
have reproduced upstream's Argon look inside our desktop: AppKit's title bars and controls and
a second menu bar, which is what the owner does not want.

### Frames (patches 0004, 0005; compositor a447cef)
- Every titled NSWindow asks for a server-side decoration (xdg-decoration) and AppKit lays it
  out as content only: no title bar, no traffic lights, no resize bands of its own. Chonkstep
  draws its frame around it exactly as around a terminal, from the active theme, at the display
  scale. That includes windows with a full-size content view (content under the title bar, as
  Calculator and Firefox have): their content starts below our title bar.
- Borderless windows (menus, tool tips), sheets and full screen windows have no standard frame
  and ask for client-side decoration.
- A compositor without xdg-decoration (upstream's `neuswc`), or one that answers client-side,
  gets AppKit's own frame as before (`PD_WS_CLIENT_FRAME=1` forces that for a test).
- Our frame's buttons keep Mac semantics. Close is xdg_toplevel.close -> `performClose:`, so
  the application is asked (`windowShouldClose:`) and may refuse. Miniaturize and maximize are
  the compositor's own operations; the compositor now tells the client through the toplevel
  states (maximized, suspended, fullscreen) and NSWindow follows: `isZoomed`,
  `isMiniaturized`, `windowDidMiniaturize:`/`windowDidDeminiaturize:`, resize notifications.
- Title: xdg_toplevel.set_title. Minimum and maximum size (and min = max for a window that is
  not resizable): xdg_toplevel.set_min_size/set_max_size.
- Not conveyed, because xdg-shell has nothing for it: document-edited state, a window without
  a close button (our button is always there; a click is still answered by the application),
  the proxy icon.

### Scale (patches 0004, 0005, 0006)
- The WindowServer takes the compositor's preferred scale from wp_fractional_scale_v1 (in
  120ths), or the output's whole scale without it. AppKit works in points: the screen is the
  output's mode divided by the scale, window sizes, positions and pointer locations are points.
- A window's backing store is points x scale pixels; the context's base transform carries the
  scale, so paths, images and glyphs are rasterised at the device density. The buffer goes to
  the compositor with a wp_viewport destination in points: at 200% it is shown pixel for pixel.
- Fractional steps (125%, 150%, 175%, 250%) are true fractional backing, not "render at the
  next whole scale and shrink": the buffer is exactly points x scale and the compositor does
  not resample it. Measured below.
- A live change (the desktop menu's Display choice) arrives as a new preferred scale; every
  window remakes its backing store and redraws, `windowDidChangeBackingProperties:` and
  `viewDidChangeBackingProperties` are sent, `NSScreen.backingScaleFactor` follows.
- Upstream's text drawing ignored every scale (glyph bitmaps at the point size, only their
  positions scaled); fixed in Onyx2D (0006).
- Not scaled yet: layer-backed content (QuartzCore composites a layer tree at 1x and the
  window context enlarges it: correct size, soft), NSOpenGLView, the pointer image an
  application sets, cached image representations made at 1x.

### Menus (proposal; nothing built, key equivalents work today)
AppKit already serialises an application's menu bar (titles, enabled and checked state, key
equivalents, submenus) for upstream's MenuBar.app and takes "item at this index path chosen"
back, over Distributed Objects. Three ways to show it on our desktop:
1. A menu strip inside our frame, under the title bar, drawn by the compositor from that
   tree (themed and scaled like the frame; the tree and the activation travel over a small
   Wayland protocol from the WindowServer instead of Distributed Objects). Each window carries
   its own menus, as our conventions have no global bar. Recommended: visible where the user
   looks, no second look, works for every Mac application unchanged.
2. A global bar at the top of the screen, as on macOS and in upstream's desktop. Costs a strip
   of every screen and is a second convention next to ours.
3. The focused application's menus as a branch of our right-click / Command-Space menu.
   Nothing on screen until asked for; cheap to add on top of (1) with the same protocol, poor
   as the only way (not discoverable).
Recommendation: (1), with (3) added afterwards from the same data.

### Firefox's title bar
Firefox by default draws its tabs in the title bar (a full-size content view) and expects the
system's buttons floating over its top left corner. Options: (a) our frame around it, tabs
directly below our title bar, no AppKit buttons (what the frame rule above gives; Firefox
finds no standard buttons and leaves no gap for them); (b) the same with Firefox's own
"Title Bar" setting, which changes nothing further here; (c) client-side frame for full-size
content windows, i.e. Firefox's look with AppKit's traffic lights, which is the forced AppKit
controls again. (a) is what is built.

## State
| Item | State |
|---|---|
| arm64 Cocoa system root | built from the pin (`build-cocoa-root.nix`, `build-desktop-root.nix` with our patches); every library reference and imported symbol resolves against itself plus the base image (`cocoa_survey.py`) |
| Minimal AppKit application (`hello/`) | runs on the desktop in QEMU arm64: window, label, button, text field, pointer, keys, menu key equivalent |
| Our frames around Mac windows, Mac semantics of the buttons | see Results |
| Scale 100% to 200% including fractional steps, live change | see Results |
| Upstream Terminal and Calculator on our desktop | see Results |
| Menus | not visible (proposal above); key equivalents work |
| Popup windows (menus, pop-up buttons, tool tips) | not handled: they are borderless toplevels placed by the compositor; needs xdg_popup in the WindowServer and in the compositor (which refuses popups today) |
| Firefox on arm64 | official 160.0, sandboxes off, software WebRender in the main process: the browser window with its chrome, a local page with text and an image, keyboard into a page's field, pointer on a page's button (12 November, below). Not yet: network pages (checks written, running), sandbox on, GPU process |
| Auto Layout | layout API on NSView and the controls, constraint activation, a Cassowary solver (patch 0011); three constraint layouts equal in structure to macOS |
| Control geometry and drawing | macOS 26 numbers and look, measured (patch 0012); compared control by control with `conformance/sheet.py` |

## Firefox on arm64 (official 160.0, QEMU, executed)
- Official Firefox 160.0 for macOS is universal (x86_64 + arm64), minimum macOS 11, LC_DYLD_INFO
  fixups (lazy binding), Developer ID signatures. Fetched at test time from Mozilla's CDN,
  checksum checked against Mozilla's SHA256SUMS, unpacked with 7-Zip from nixpkgs; never committed.
- Static survey (`cocoa_survey.py`, `runs/cocoa/firefox/survey-root2.md`): 27 Mach-O images, 45
  system libraries, 2413 imported symbols. Against `build-firefox-root.nix` (our root, upstream's
  small frameworks built for arm64, Security, SystemConfiguration, our Metal/MetalKit/Accelerate,
  patches 0001 to 0003): no library absent, 19 symbols missing, none of them stops XUL at load
  (the load-time gaps left are in the updater and installer helpers).
- How far it gets, same result with the default sandbox and with Mozilla's switches
  (`runs/cocoa/ff-sandbox`, `runs/cocoa/ff-nosandbox`):
  1. `firefox` starts: the Developer ID signed executable and libmozglue load and run on our
     kernel without any code-signing refusal.
  2. XUL loads (before the additions: `dlopen(XUL): image not loaded`, `runs/cocoa/f1`) and
     initialises: the process connects to the compositor (`WindowServer: scale 120/120 ...
     frames by the compositor`), AppKit starts (`firefox[64] *** Unknown color catalog Basic`).
  3. A child process is started and reaches its sandbox set-up (default sandbox run only).
  4. NSApplication runs: Firefox creates its first window, a hidden 100x100 borderless helper
     (`window ... level 2 frame {{0, 668}, {100, 100}}`), and its menu is installed
     (`NSApplication: waiting for the menu bar server`).
  5. About two minutes after start the main process dies with SIGSEGV (exit 139), before any
     browser window. No page, no input, no network. About 3.5 minutes of emulation per attempt.
     Cause found with the Homebrew stream's `libhbcrash` (`runs/cocoa/ff-crash`): a background
     thread calls `SecItemCopyMatching` and then `CFArrayGetCount` on the result (XUL+0x154841c,
     +0x1548430; fault address 0x20 in `objc_msgSend`). Our Security answered success without
     writing the result when no keychain daemon runs, so Firefox used an uninitialised
     pointer. Patch 0009 defines the result on every path and answers `errSecItemNotFound`
     for an empty search, as an empty keychain does on macOS.
  6. With 0009 (`runs/cocoa/ff-trace`): the main process lives on, its GPU helper cannot start
     (below), Firefox falls back by itself (`Fallback WR to SW-WR`, `Failed to launch GPU process
     after 3 attempts`), then aborts on a lazily bound `proc_name` (in libproc at the pin, not
     exported).
  7. With `proc_name` exported and upstream's per-user directory helper (`runs/cocoa/ff-trace2`,
     the last run): the main process runs to the end of the session (13 minutes), creates its
     browser window object (`window ... level 0 frame {{0, 767}, {1, 1}}`) and never sizes or
     shows it; nothing of Firefox is on the screen (`shots/05.png`). Its child processes die or
     hang: 289 child starts stop before `sandbox_init` with an empty `Sandbox error:` (a silent
     `return false` in Mozilla's set-up; `confstr(_CS_DARWIN_USER_CACHE_DIR)` was one cause and
     is fixed for the content process, the cause for the others is not found yet), and the
     content process that does get its sandbox is denied the compositor's socket by it (967
     `deny network-outbound .../wayland-0`).
  So today: binary loads, XUL initialises, AppKit and the menu come up, the browser window
  object exists; no window on screen, no page, no input, no network.
- The sandbox, both paths:
  - Default (sandbox on). A child's `sandbox_init_with_parameters` is refused by the profile
    compiler: at the pin `define expects a name and a value` (seen on x86_64 with 158); with
    upstream's tip compiler (patch 0007) `line 22: unbound variable 'default'` (Mozilla passes
    an operation name to its own function). Firefox prints the profile and the error; the
    child then ends (Mozilla treats a failed sandbox as fatal for that process) and the parent
    carries on. Nothing here pretends the sandbox succeeded.
  - Patch 0008 extends the compiler (operation names as values, string functions, numbers and
    comparisons, when/unless, defined?, a profile's own function as a filter; require-all,
    require-not and with-filter compiled to the strict side: what cannot be checked makes a deny
    rule broader and leaves an allow rule out). On the build host, against Mozilla's own
    sources for 160 (`security/sandbox/mac/SandboxPolicy*.h`), all six profiles now compile:
    content 1993 bytes, socket 1494, GPU 656, RDD 651, GMP 497, utility 387; with the tip's
    compiler alone none of the six did. In the guest the kernel then really enforces the
    compiled profiles on Firefox's children: the socket process (`deny file-read-data
    /etc/master.passwd`: our libc reads the password file where macOS asks a daemon) and the
    content process (`deny network-outbound /private/var/run/qemu-desktop/wayland-0`: on macOS a
    content process reaches the window server through a Mach service that Mozilla's profile
    allows; our window server connection is a Unix socket the profile knows nothing of). The
    honest fix for the second is in the system, not in Mozilla's profile: treat the profile's
    permission for `com.apple.windowserver.active` as permission for the compositor socket. Not
    built.
  - Disabled (`MOZ_DISABLE_CONTENT_SANDBOX=1`, `..._GMP_`, `..._RDD_`, `..._SOCKET_PROCESS_`,
    `..._UTILITY_`, `..._GPU_SANDBOX=1`, Mozilla's documented switches): no sandbox error; before
    patch 0009 the same crash at the same place, so that crash was not the sandbox. Not yet run
    again after 0009: it is the next run to make, since it takes the sandbox out of the way of
    the first visible window.
- 12 November, the first visible window and page (sandboxes off, `checks/firefox-window.json`,
  `firefox-page.json`; runs `ff-w2`, `ff-p3`, screenshots in their `shots/`):
  - Why nothing was shown before was not sizing. Firefox maps its window 1 by 1, hides it, sizes
    it, shows it. The WindowServer library answered a configure sent before the unmap; the
    compositor resets a toplevel on unmap and closed the connection (`xdg_wm_base error 4: wrong
    configure serial`). Now (patch 0004): configures that arrive before the compositor has
    passed an unmap (a `wl_display.sync` barrier) are dropped unanswered; a remapped toplevel
    does the empty commit and waits for its configure; a window is not mapped with a frame from
    before it was resized; a connection the sandbox refuses is final (the process is headless,
    no 60 s retry per call).
  - Compositor (worktree commit 25ec50c and the one after): a new window larger than the work
    area opens at the size that fits, frame included, and one that grows itself past it later
    is held to it (a backend capability, on for wm-darwin only); a window remapped at another
    size is placed afresh.
  - `ff-w2`: window with frame, tab strip, address bar, toolbar, about:blank. `ff-p3`: the page
    `guest/firefox-page/index.html` (heading, paragraph, a PNG, a coloured block, a field, a
    button) drawn by Gecko through WebRender's software renderer; the page reports through the
    window title: `SF page: loaded image=160x100 typed=abc clicks=1` after the driver typed
    `abc` and clicked the button; the block is 160 pixels wide for 160 CSS pixels at 100%.
  - Conditions: all six `MOZ_DISABLE_*_SANDBOX` switches; `guest/firefox-profile/user.js`
    (documented preferences only: `gfx.webrender.software`, `layers.gpu-process.enabled=false`,
    one content process, no first-run pages, no background traffic, and the first-run Terms of
    Use sheet switched off as Mozilla's test harnesses do - a check does not accept terms for
    anyone; whoever uses this Firefox for real starts it without the file). Firefox itself
    shows "The security sandbox is disabled" in the window.
  - Open: the GPU process (starts with sandboxes off, tries OpenGL on Mesa's software
    rasteriser, is killed by Firefox for an IPC timeout under emulation); one run of five
    (`ff-ns3`) died in the parent with `pthread_cond_wait` returning EINVAL (not reproduced,
    cause not found); in `ff-p3` the window was first mapped at 921x691 and then grew itself
    off the screen's right edge (fixed after that run in the compositor, not yet seen fixed).
  - Sandbox on, design (not built): on macOS Firefox's content processes have no window server
    connection at all (`HAS_WINDOW_SERVER` is FALSE in the content profile), so the content
    process must simply be headless here, which the refused-connection change gives. For
    processes whose profile does allow `com.apple.windowserver.active`, libsandbox should add
    an outbound rule for the compositor's socket path when it compiles that permission: the
    translation in the layer that knows the window server is a socket here, kernel enforcement
    unchanged. A broker passing a connected descriptor needs the parent's cooperation, which
    Firefox does not give.
- Still ahead after the crash: layer-backed rendering through QuartzCore (software only in
  QEMU; on the M1 Firefox's WebRender would use OpenGL through Mesa/AGX, there is no Metal
  device), popup windows (its menus and panels are borderless child windows), fonts, HTTPS and
  the trust store (tip work, not in the pin), the content process model.

## Patches and their state (`integrations/puredarwin/cocoa/patches`, against the pinned tree)
| Patch | What | From | At upstream's tip (rebase stream's inventory) |
|---|---|---|---|
| 0001 | AppKit: NSFontWeight constants, NSImageSymbolConfiguration | tip 71a9d0b0 (part) | already there: drop |
| 0002 | AppKit: NSSharingServicePicker | ours | applies |
| 0003 | AuthenticationServices: browser side of ASWebAuthenticationSession | ours | applies |
| 0004 | WindowServer: xdg-decoration, scale (wp_fractional_scale + wp_viewporter), size limits, toplevel states | ours | needs porting |
| 0005 | AppKit: no title bar in a compositor frame, scaled backing stores, backing-change notifications, zoomed/miniaturised from the compositor | ours | needs porting |
| 0006 | Onyx2D: glyphs rasterised at the device scale | ours | needs porting |
| 0007 | libsandbox: SBPL functions, imports, named profiles | tip 1ebd97cb (part) | already there: drop |
| 0008 | libsandbox: the rest of the profile language Firefox uses, strict-side handling of what cannot be checked | ours | not checked |
| 0009 | Security: SecItemCopyMatching defines its result, errSecItemNotFound for an empty search | ours | not checked (the file is unchanged at the tip) |
| 0010 | libSystem: per-user local, temporary and cache directories | tip 1ebd97cb (part) | already there: drop |
| (recipe) | libSystem: `proc_name` exported | tip 7f10e1a1 (one line) | already there: drop |
| 0011 | AppKit: layout API, constraint activation, Cassowary solver (`PDLayoutSolver`, written here from the Badros/Borning/Stuckey paper), whole-tree layout in y-down coordinates, `-layout` called on resize | ours | not checked; the tip's AppKit has none of it (it adds NSStackView only) |
| 0012 | AppKit: control geometry and drawing of macOS 26 as measured (push, checkbox, radio, pop-up, field, slider, progress, segmented); the macOS 15 look kept behind the default `PDControlLook` = `15`; a string is drawn in a rect lower than its line | ours | not checked |
| 0013 | Onyx2D: PNG encoder writes straight alpha and reads "default" byte order as RGBA (pictures of bitmap contexts had red and blue exchanged) | ours | not checked |
0004 also carries the unmap/remap fixes and the headless rule; 0005 the scaling of
`cacheDisplayInRect:` to its bitmap (was 1x in a corner of a 2x bitmap).
Tip variants of 0004 to 0006 are not written yet (to go in `patches-tip/` when the rebase
stream has a tip build).

## Implementations added that answer "nothing here" (complete list)
| Symbol | What it does |
|---|---|
| `MTLCreateSystemDefaultDevice`, `MTLCopyAllDevices`, `MTLCopyAllDevicesWithObserver` | nil / empty array / empty array and nil observer: there is no Metal device |
| `MTLCaptureManager` | `supportsDestination:` NO, `startCaptureWithDescriptor:error:` NO with MTLCaptureErrorNotSupported, no scopes |
| MetalKit.framework | a library with no symbols: Firefox's libmozinference names it as a dependency and imports nothing from it |
| `ASWebAuthenticationSessionWebBrowserSessionManager` | keeps the handler; `wasLaunchedByAuthenticationServices` NO; nothing starts sessions on this system |
| `NSSharingServicePicker` | menu of the services the delegate proposes (the system has none); with none, shows nothing and tells the delegate nil was chosen |
| `xdg_toplevel` configure_bounds and wm_capabilities in the WindowServer | received and ignored |
Not stubs: the Metal descriptor classes, `ASWebAuthenticationSessionRequest`, the ten vDSP
routines, the sandbox compiler additions (rules it cannot express are made stricter, never
reported as applied when they are not), `SecItemCopyMatching` (reports an empty keychain).

## Missing, in the order it blocks things
1. Firefox: why most child processes stop before `sandbox_init`; the compositor socket for
   sandboxed content processes; then the first visible window (run with Mozilla's sandbox
   switches after patch 0009 to separate the two), layer-backed rendering, HTTPS and trust.
2. Popup windows (menus, pop-up buttons, tool tips, Firefox's panels): xdg_popup in the
   WindowServer and in the compositor, which refuses popups today.
3. Menus: the owner's choice between the three proposals above.
4. Auto Layout beyond patch 0011: layout guides, NSStackView (tip only), constraints from nibs
   exercised; the font tracking table and the missing weights; the rest of the control look
   (see the second conformance pass).
5. Scale: layer-backed content, NSOpenGLView and application-set pointer images are 1x;
   offscreen caches render at 1x.
6. For Chromium applications (Codex, Claude): remote layers across processes in our
   compositor (subsurfaces, `pd_surface_host`), about 50 libraries of missing imports that
   must all resolve at load, a GPU path without Metal.
7. Not conveyed to our frame: document-edited state, absent close button.
8. Seen, not investigated: upstream Terminal's text at about half the cell size.

## Plan for the M1
Nothing here has run on the M1. The pieces are arm64 binaries built for the same userland, so
the first M1 boot is: the root of `build-desktop-root.nix` and the libSystem of
`build-libsystem-cocoa.nix` in the RAM root, the compositor from branch `wm-darwin-cocoa`,
`cocoa_check.sh`'s hello and apps checks by hand at 200% on the panel (2560x1600, so a
1280x800 point desk; QEMU could only show 512x384 points at 200%). To look for there and not
visible in QEMU: glyph and frame sharpness on the real panel, redraw speed of AppKit's
software path at 2560x1600, the GL compositor path with shm clients that carry a viewport,
trackpad scrolling into AppKit (scroll events were not tested here at all), the SPI keyboard's
modifier keys as key equivalents. Mesa/AGX OpenGL for Firefox's WebRender comes after Firefox
shows a window in QEMU.

## How to run
```sh
integrations/puredarwin/cocoa/cocoa_check.sh hello --scale 2 --theme graphite    # one check, exit 0 = pass
integrations/puredarwin/cocoa/cocoa_check.sh frame --scale 2
integrations/puredarwin/cocoa/cocoa_check.sh live-scale
integrations/puredarwin/cocoa/cocoa_check.sh apps --scale 1.5 --theme amber-phosphor
```
`cocoa_check.sh` builds the root (`build-desktop-root.nix`) and the Homebrew stream's libSystem
(`../homebrew/build-libsystem.nix`: the `___chkstk_darwin` fix), makes the image
(`cocoa_image.py`: the QEMU stream's template with the root grown to 3 GiB) and runs the steps
of `checks/<name>.json` (`cocoa_run.py`: QMP pointer and keys, console markers, measurements
from the framebuffer). The compositor is `runs/cocoa/target/...` when built from the worktree
(as `tools/qemu_desktop.sh --src` builds it), else the M1's current binary.

## What should go into the default images
- Patches 0004 to 0006 with the root of `build-desktop-root.nix` (about 180 MB: frameworks,
  fonts, Mesa, LLVM for Mesa's software rasteriser, Swift runtime for Foundation's compat
  framework).
- Compositor commits a447cef and 06d0ea9 (branch `wm-darwin-cocoa`): toplevel states for
  maximise/minimise; a test option for the autopilot's scale phase.
- The default libSystem (`../libsystem-default`); for Firefox additionally patches 0007, 0008,
  0010 and the `proc_name` export (`build-libsystem-cocoa.nix`), and patch 0009 in Security.

## Results (QEMU arm64, 1024x768 framebuffer; every row executed, run directory under `runs/cocoa/`)
Checks are `cocoa_check.sh <check>`; "px/pt" is measured on the framebuffer from a marker the
application draws; "ink" is the pixel height of the first two text lines.

| Check | Scale, theme | Result | Measured |
|---|---|---|---|
| hello (frame, button, pointer, keys, key equivalent) | 100%, nextstep-classic | pass 9/9 | 1.000 px/pt, ink 14/13 |
| hello | 125%, jade-lacquer | pass 9/9 | 1.250 px/pt, ink 15/15 |
| hello | 150%, amber-phosphor | pass 9/9 | 1.500 px/pt, ink 19/19 |
| hello | 200%, graphite | pass 9/9 | 2.000 px/pt, ink 25/24 |
| frame: our close button asks the application (refused once, then allowed, application ends) | 200% graphite; 100% nextstep-classic | pass 8/8 both | |
| miniaturize: our button -> `windowDidMiniaturize:`, `isMiniaturized` | 100% | pass | |
| zoom: Ctrl+Shift double click on our title bar -> resize and `isZoomed` | 200% graphite | works when the double click registers: seen in two runs (`zoom-evidence-s2-graphite`, `zoom-s2-graphite`: 510x351, `zoom-state zoomed=1`). Under emulation the compositor's 400 ms double-click window is met about one time in five, so the check itself is unreliable; restoring by a second double click has not been observed | |
| apps: upstream Terminal (a typed command ran in its shell) and Calculator (the + key lights when clicked), both in our frames | 100% nextstep-classic, 150% teal-blueprint, 200% graphite | pass 9/9 each | Calculator 1.01 / 1.52 / 2.03 px/pt |
| live scale change (the desktop steps 100 -> 125 -> 150 -> 175 -> 200 -> 250 -> 300 -> 100%, 25 s each) | teal-blueprint | pass 10/11 (`live-scale-teal-blueprint`): `windowDidChangeBackingProperties:` with 1.250, 1.500, 1.750, 2.000, 2.500, 3.000, 1.000 in that order, a screenshot at each. The one failed check: the compositor counted 3 commits at a stale scale over the 7 changes (a frame already under way when the scale changed; at 250% one 2.0 buffer was shown resampled once). Two faults were found and fixed to get here: the notification was skipped when a resize had rebuilt the backing first, and `NSWindow.backingScaleFactor` read 1 while AppKit's frame lay off the screen in points | |
| no resampled commit at a fixed scale | all of the above | pass | |

Two faults of the test tooling were found and fixed, and the affected runs repeated: two
checks had matched their own echoed command line on the console (false passes of the metrics
run and of the Terminal step); and the image cache was keyed by file sizes and times only, so
a rebuilt framework of the same size was taken for the old one (one live-scale rerun ran the
old AppKit). The table has only results from after both fixes where they matter.

Seen and not fixed: upstream Terminal draws its text at about half the cell size at every scale
(also at 100%, so not a scaling fault; not investigated); an application-set pointer image stays
at 1x; windows are placed by the compositor, the position AppKit asked for is ignored.

## Conformance against real macOS (first scorecard)
Reference: macOS 26.5.2 on the bench controller, used only as a renderer in `~/sf-cocoa-ref/`
over ssh (no GUI session touched: an NSWindow that is never shown and `cacheDisplayInRect:`
draw at 2x headless). `conformance/metrics.m` builds unchanged on both sides;
`conformance/compare.py` compares the JSON. Raw files: `runs/cocoa/ref-macos/`,
`runs/cocoa/conformance/` (`scorecard-001.md`, `side-by-side-001.png`, `diff-001.png`).
No Apple font or artwork was copied anywhere; the reference PNGs stay under `runs/`.

1548 values compared: 382 equal within 0.01 pt, 867 different, 299 missing on our side.

| Area | Finding |
|---|---|
| System font | upstream ships Inter (Regular and Bold only) as the system font. Against macOS's system font: ascender within 0.03 pt at every size; descender 0.3 to 0.8 pt deeper; cap height +3.3%, x-height +3.7%; default line height equal at 9, 10, 12, 13, 14, 17 pt and 1 pt taller at 11, 15, 20, 24, 28 pt |
| String advances | Inter has no size-dependent tracking: a 37-character label is 4.1% narrow at 9 pt, 1.2% wide at 13 pt (+2.6 pt), 5.9% wide at 17 pt, 12% wide at 28 pt (+49 pt) |
| Tracking table (computed, not yet applied) | least-squares tracking per size that brings Inter to the reference, in 1/1000 em: 9: +15.4, 10: +8.6, 11: +2.8, 12: -3.1, 13: -9.0, 14: -13.8, 15: -18.7, 17: -28.5, 20: -40.1, 24: -50.6, 28: -51.1. Predicted residual on the text strings after it: within 2 pt up to 15 pt size, within 4 pt at 28 pt; digits stay 1.5 to 14 pt narrow and capitals wide, because the glyph proportions differ. Advances cannot be made identical without Apple's fonts |
| Weights | ultraLight, thin, light resolve to Inter-Regular; medium, semibold, heavy, black to Inter-Bold (macOS has nine distinct weights) |
| Monospaced | Nimbus Mono PS where macOS has Menlo / SF Mono; `monospacedDigitSystemFontOfSize:weight:` missing |
| Control sizes (regular) | push button 92x32 (macOS 80x24), checkbox 113x18 (110x16), pop-up 106x22 (119x24), text field height 22 (24), segmented 136 (148) wide, slider `sizeToFit` returns 10000x10000 |
| Layout API | `intrinsicContentSize`, `fittingSize`, `alignmentRectInsets`, `firstBaselineOffsetFromTop`, `baselineOffsetFromBottom` do not exist on NSView (270 of the 299 missing values): Auto Layout based applications cannot lay out against this AppKit |
| Colours | 10 of 20 system colours equal; different: disabledControlTextColor, controlColor, placeholderTextColor, controlTextColor, keyboardFocusIndicatorColor, windowBackgroundColor and four more |
| Window | titled frame for 400x300 content: macOS 400x332; ours 400x300 here because the compositor draws the frame (by design) |
| Pixels (one view tree, inactive window, 1x) | 6.7% of the pixels differ by more than 8%; structural dissimilarity 0.10. Visible in `side-by-side-001.png`: older control artwork (bordered capsule button, square checkbox, square field), slider knob drawn at 0 for a value of 0.4, the offscreen render on our side is at 1x (`renderScale` 1 against 2) |

Order of work this gives: layout API on NSView (blocks every modern application), control
geometry to the reference, the tracking table and the missing weights (Inter has all nine
as static or variable fonts under the SIL Open Font License), then control drawing. The
conformance suite so far is this one program; the areas the owner listed (tables, menus,
toolbars, sheets, Auto Layout, dark appearance, ...) are not written yet.

### Second pass (12 November): layout, control geometry, control look
- Layout. At the pin and at the tip alike NSLayoutConstraint was data, AppKit relaxed required
  equalities among a view's direct subviews, `setActive:` set a flag only (nothing made with
  anchors was laid out), and "top + 20" went up in unflipped views. Patch 0011 replaces that.
  Three constraint layouts are in the suite (`layout` in the JSON); against macOS their
  structure is equal (label at x 18 on both, a field stretched 20..300 on both, fitting sizes
  from the same sums), what differs is the width of text in Inter.
- Values: 2077 compared, 1214 equal within 0.01 (first scorecard: 382 of 1548), 29 missing
  (299). `runs/cocoa/conformance/scorecard-007.md`.
- Control sheet: `metrics <path>` also renders every control at the three control sizes, once
  in an ordinary unshown window (inactive look) and once in a window that says it is key
  (active look, still never shown); `conformance/sheet.py` cuts each control out of both
  pictures by its own frame and compares. 45 controls on each sheet
  (`sheet-sheetActive-003.md`/`.png`, `sheet-sheet-003.md`/`.png`): 22 frames the same size and
  the rest within 2 points (Inter's widths); mean difference 9.9 of 255 (active) and 10.0
  (inactive); 7.7% and 7.6% of pixels off by more than 24 (the run before the slider and
  accent fixes: 14.4%). Sliders 0.4% off, progress bars and disabled buttons a few percent;
  the worst are the labels and titles (14 to 17%), which is the glyphs: Inter against the
  system font, drawn about a point lower. Also left: the field's ring (macOS draws it one
  point outside the frame; a cell here cannot), the slider knob's halo outside the frame
  (same reason), heavier pop-up arrows.
- Two looks: the macOS 26 look is the default; `PDControlLook` = `15` (or `PD_CONTROL_LOOK=15`)
  gives upstream's macOS 15 look in the same geometry. Dark appearance and pressed states of
  the 26 look are not measured (noted in `PDControlDrawing.m`).
- Not yet: tracking table, missing weights, dark appearance, tables, menus, toolbars, sheets.

## Claude and Codex desktop applications (static survey)
- Codex: OpenAI's documented download (`persistent.oaistatic.com/codex-app-prod/Codex.dmg`,
  896 MB, sha256 488009cf...ad4423a) contains `ChatGPT.app` (bundle id com.openai.codex,
  26.1106.51428, macOS 13.0 or later, arm64 only): OpenAI's own Chromium runtime ("owl",
  Chromium 157.0.8103.51, `app.asar`), Sparkle, a bundled Node 24.22.1 and Codex CLI, helper
  applications, App Sandbox entitlements on every executable. Survey
  (`runs/cocoa/apps/codex/survey.md`): 91 Mach-O images, 119 system libraries, 4670 imported
  symbols, 817 missing, 48 libraries absent. The runtime uses chained fixups, so every import
  binds at load; it alone has gaps in 49 libraries.
- Claude: not obtained. Anthropic's documented download links answer 403 to an automated
  client; a person has to fetch `Claude.dmg` with a browser into `runs/cocoa/apps/claude/`.
- Beyond linking, Chromium needs: remote CoreAnimation layers into the window (upstream's
  `pd_surface_host_v1`; our compositor has neither that nor subsurface compositing: missing),
  the sandbox (kernel extension present; newer profile compiler carried as patch 0007;
  `--no-sandbox` is Chromium's switch), a GPU path without Metal (ANGLE on OpenGL through
  Mesa, or SwiftShader), IOSurface (framework and a launchd job for upstream's iosurfaced are in the root; untested),
  Mach bootstrap/XPC (present, untested), Keychain and trust (Security present, trustd
  missing at the pin), LaunchServices URL schemes (untested), audio (lab coreaudiod; AVFAudio
  absent), accessibility and screen capture (missing).
- Order: Firefox 160 first, then an official Electron release's blank window as the Chromium
  stepping stone, then Codex and Claude. The command-line tools (Claude Code, Codex CLI) are
  a separate stream's work and the short route.
