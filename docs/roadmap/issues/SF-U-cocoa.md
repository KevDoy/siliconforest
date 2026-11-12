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
| Firefox on arm64 | official 160.0: `firefox` starts, XUL is refused by dyld (`runs/cocoa/f1`); the link gaps are known from the survey and partly closed in source (below), not yet rebuilt and rerun |

## Firefox on arm64: what is known
- Official Firefox 160.0 for macOS is universal (x86_64 + arm64), minimum macOS 11, LC_DYLD_INFO
  fixups, Developer ID signatures. Fetched at test time from Mozilla's CDN, checksum checked
  against Mozilla's SHA256SUMS, unpacked with 7-Zip from nixpkgs; never committed.
- Executed (`runs/cocoa/f1`): `/Applications/Firefox.app/Contents/MacOS/firefox` starts on our
  kernel (the signed main executable and libmozglue load and run; no code-signing refusal),
  then `dlopen(XUL): image not loaded`, `Couldn't load XPCOM`.
- Static survey (`cocoa_survey.py`, `runs/cocoa/firefox/survey-root1.md`): 27 Mach-O images,
  45 system libraries, 2413 imported symbols. Against our root plus upstream's small frameworks
  for arm64 (`arm64.nix`), Security and SystemConfiguration: 52 symbols missing, of which these
  stop XUL at load: Metal.framework (absent on arm64), `ASWebAuthenticationSessionErrorDomain`,
  `ASWebAuthenticationSessionRequest`, `ASWebAuthenticationSessionWebBrowserSessionManager`,
  `NSFontWeightRegular`, `NSImageSymbolConfiguration`, `NSSharingServicePicker`. The rest are
  lazily bound functions (printing, PDF, `proc_name`, `CFBundleIsArchitectureLoadable`) that fail
  only if called.
- Written, not yet built into a root: patches 0001 to 0003 (the AppKit and
  AuthenticationServices symbols), `frameworks/Metal` (a Metal.framework that truthfully reports
  no device: `MTLCreateSystemDefaultDevice()` nil, `MTLCopyAllDevices()` empty, descriptor
  classes as value objects), `frameworks/Accelerate` (the ten vDSP routines libmozinference
  imports, real loops).
- Expected next obstacles, from upstream's x86_64 run of 158: the content-process sandbox
  profile that libsandbox cannot parse, NSStackView for the crash reporter, then rendering
  (layer-backed content through QuartzCore; software only in QEMU).

## Implementations added that answer "nothing here" (complete list)
| Symbol | What it does |
|---|---|
| `MTLCreateSystemDefaultDevice`, `MTLCopyAllDevices`, `MTLCopyAllDevicesWithObserver` | nil / empty array / empty array and nil observer: there is no Metal device |
| `MTLCaptureManager` | `supportsDestination:` NO, `startCaptureWithDescriptor:error:` NO with MTLCaptureErrorNotSupported, no scopes |
| `ASWebAuthenticationSessionWebBrowserSessionManager` | keeps the handler; `wasLaunchedByAuthenticationServices` NO; nothing starts sessions on this system |
| `NSSharingServicePicker` | menu of the services the delegate proposes (the system has none); with none, shows nothing and tells the delegate nil was chosen |
Not stubs: the Metal descriptor classes, `ASWebAuthenticationSessionRequest`, the vDSP routines
(real behaviour).

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
- Compositor commit a447cef (branch `wm-darwin-cocoa`).
- The Homebrew stream's libSystem, which this stream uses unchanged.

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
| zoom: Ctrl+Shift double click on our title bar -> resize and `isZoomed` | 200% graphite | shown once (`zoom-evidence-s2-graphite`: 510x351, zoomed=1); the restore and a second run at 100% did not register the double click (its 400 ms window does not always survive emulation); the check now repeats the click, rerun pending | |
| apps: upstream Terminal (a typed command ran in its shell) and Calculator (the + key lights when clicked), both in our frames | 100% nextstep-classic, 150% teal-blueprint, 200% graphite | pass 9/9 each | Calculator 1.01 / 1.52 / 2.03 px/pt |
| live scale change (the desktop steps 100 -> 125 -> ... -> 300 -> 100%) | | 125, 150, 175% followed with `windowDidChangeBackingProperties:`; at 200% the window rendered at 2.0; at 250% and 300% the compositor also shrank the window to fit and the notification was skipped (fixed: `_reportedScale120`); the compositor counted 3 commits at a stale scale during the 7 changes (frames already under way). Rerun with the fix pending | |
| no resampled commit at a fixed scale | all of the above | pass | |

Two checks had matched their own echoed command line on the console (false passes of the
metrics run and of the Terminal step); the markers were changed and those runs repeated; the
table has only the repeated results.

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
  Mesa, or SwiftShader), IOSurface (framework present, daemon not started, untested),
  Mach bootstrap/XPC (present, untested), Keychain and trust (Security present, trustd
  missing at the pin), LaunchServices URL schemes (untested), audio (lab coreaudiod; AVFAudio
  absent), accessibility and screen capture (missing).
- Order: Firefox 160 first, then an official Electron release's blank window as the Chromium
  stepping stone, then Codex and Claude. The command-line tools (Claude Code, Codex CLI) are
  a separate stream's work and the short route.
