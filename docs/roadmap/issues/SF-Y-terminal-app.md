# SF-Y: Apple's Terminal.app on SiliconForest (GitHub issue 52)

## Goal
The unmodified Terminal.app binary from the M1's own macOS launches and works on the SiliconForest
desktop: the demonstration that macOS applications run here. The binary is never committed; every
step that makes it run is part of SiliconForest. Phases: 1 measure (done by the coordinator),
2 load (dyld reaches `main`), 3 launch (a window on the compositor), 4 a working shell in it.

Test copy: `private/terminal-app/m5-26.5.2/Terminal.app` (macOS 26.5.2, from the controller Mac;
git-ignored). The final binary comes from the M1's macOS 27.0: nothing below may depend on one build.

## State (13 November 2026, evening)
**On the M1, from a scratch prefix (`/private/tmp/sfy`, nothing installed, the system's own dyld,
libSystem, Foundation untouched): the unmodified arm64e code of Apple's Terminal 2.15 loads, runs
its `main`, loads its nibs, opens its window on the running compositor and starts a shell on a pty.
The window is white: no text is drawn yet.** Not run in QEMU at all (the owner made the M1 the
instrument on 13 November).

| Phase | State | Evidence |
|---|---|---|
| 2 load | done on the M1 | scanner: 733 imports, 0 missing (`homebrew/gapscan/terminal-after.md`); dyld binds all of them and runs the initializers |
| 3 launch | done on the M1 | WindowServer trace of the process (`runs/sf-terminal/final-run.txt`): `scale 180/120 (fractional), screen 1707x1067 points of 2560x1600 pixels, frames by the compositor`; `window … level 0 frame {{20, 688}, {524, 327}} title "Terminal — sh — 80×24"`; `toplevel … frame by the compositor`; `keyboard enter surface` |
| 4 shell | partly | the child `sh` runs on `/dev/ttys002` with Terminal as parent (`ps`), the title follows the process name; text written to the pty makes the view redraw and a cursor block appears, cut off at the top edge (`runs/sf-terminal/shots/m1-window-002.png`); **no glyph is drawn; typing was not tried** |

The pictures are what the application handed to the compositor (`PD_WS_DUMP_DIR`, patch 0022), not
a capture of the screen: the compositor on the M1 has no screen capture. Nobody looked at the panel.

What "test copy" means: the kernel starts the dynamic linker a program names. Until the system's
`/usr/lib/dyld` has the patches below, the test runs a copy of the binary's arm64e slice in which the
13 bytes `/usr/lib/dyld` read `/tmp/sfy/dyld`. Nothing else of the binary is changed; with the patched
dyld installed the unmodified bundle runs (not done: that is the guarded replacement).

### What stands between this and "perfectly working" (in the order met)
| Item | What is known | Estimate |
|---|---|---|
| Text is not drawn | `-[TTView drawAttributedString:forLine:…]` runs without its assertion now; the cursor is drawn one row above the visible area, so the view's flipped coordinates or the origin of the first row are wrong, and glyphs (`CTFontDrawGlyphsWithAdvances` in `compat/CoreText.c`, `CTLineDraw`, an NSFont used as a CTFont through `-[NSFont forwardingTargetForSelector:]`) are untested | 2 to 4 days |
| Keyboard into the shell | the window gets keyboard focus; Terminal takes keys through `NSTextInputClient` and writes to the pty: untested | 1 to 2 days after text |
| `-[NSApplication setDefaultHelpBookSearchEnabled:]` | the one unanswered message of a launch (reported, launch goes on) | minutes |
| Preferences window, profiles | about 40 key-value coding and bindings failures at launch (`selectedTag`, `contentArray`, `enabled` set to nil …): PD's KVC does not find the accessors Apple's does, the binder lacks several bindings. Terminal reads its profiles; the built-in "Basic" profile is what shows | 1 to 2 weeks |
| Main menu | loads from the nib; not visible (SF-U's menu proposal is not built); key equivalents untested | SF-U item |
| Symbol images, `Assets.car` | `Could not find image named 'gearshape'` and others: SF Symbols and the CoreUI catalogue are not read; AppKit's own `NSColorPanel.nib` is missing from its resources | 1 week for a `.car` reader, symbols need an open symbol font |
| 213 selectors no class implements (static) | 130 of them are delegate callbacks Terminal answers itself; the rest belongs to tabs, Look Up, Quick Look, help, find bar, bookmarks | as they are met |
| Sound | `AudioServices*` answer errors: the bell is silent | 2 days over the sound stream's server |
| Private frameworks | DataDetectorsCore (no links found in output), CoreAnalytics (nothing collected), libScreenReader (empty): by design | - |
| Scrollers | `NSScrollerImpPair` (private) answers nothing; Terminal's scroller is driven through it | 3 days |
| Swift | the 5.4.3 overlay subset plus `swift/SFBridging.swift`; nothing of Terminal's Swift code beyond `swift_getObjectType` at launch was seen running | unknown |
| Install | dyld, libSystem, libFoundation are files every program loads: QEMU pass incl. fresh boot, then the guarded procedure; AppKit, libWindowServer, the wrappers and new libraries can go on directly | 1 day once QEMU is usable |
| The M1's own Terminal (macOS 27.0) | not looked at; nothing here names a symbol address or an offset of the 26.5.2 build | rescan |

## The binary (scanner, `integrations/puredarwin/homebrew/sf_gapscan.py --app`)
The scanner now reads universal files (`--arch arm64e`), the import table of chained fixups, the
chains themselves (authenticated binds and rebases), Objective-C class and category method lists and
`__objc_selrefs`, and takes an application bundle that is not in the root (`--app <dir>=<logical path>`;
`--under <tree>` lays a tree under the root). Table of 13 November against the M1's disk root as it was:
`integrations/puredarwin/homebrew/gapscan/terminal-before.md`.

| | |
|---|---|
| Slices | x86_64, arm64e (ptrauth ABI byte 0x80); chained fixups, pointer format 12 (`DYLD_CHAINED_PTR_ARM64E_USERLAND24`) |
| Fixups | 11068 rebases (688 authenticated), 2438 binds (1902 authenticated); 442 imports bound authenticated (IA 360, DA 84, all address-diversified) |
| Imports | 733 from 37 libraries (16 weak Swift overlays, none of which it imports a symbol from) |
| Missing on the M1's disk, 13 November | 214 symbols; libraries absent: libswiftCore (83), ColorSync (11), DataDetectorsCore (4), ApplicationServices (2), CoreAnalytics (2), libScreenReader (0); in libraries that are there: AppKit 37, Foundation 16, CoreFoundation 14, libSystem 13, CoreGraphics 11, AudioToolbox 6, Carbon 6, CoreServices 5, QuartzCore 2, CoreText 2 |
| Objective-C | 144 classes imported, 16 not defined anywhere; 3271 selectors sent, 307 implemented by no image of the closure (a lower bound of what is missing: a selector some class has may be missing on the class it is sent to) |

## arm64e: decision
**Pointer authentication stays off, for every process, as upstream PureDarwin already has it; arm64 and
arm64e images share a process.** Not a scheme of ours: upstream's kernel (in our tip, so in the installed
kernel `093b61f1…`) keeps the PAC keys off (`machdep.ptrauth_enabled: 0`), rewrites PAC instructions to
their plain forms in every page that is faulted in executable (`pd_pac_lower_page`, `osfmk/vm/vm_fault.c`;
a private copy for file mappings so the file keeps its signed page) and emulates what cannot be rewritten
(`LDRAA/LDRAB`, `PACGA`: `osfmk/arm64/sleh.c`); upstream's dyld is an arm64 program compiled with
`-fptrauth-calls`, so its signing of authenticated fixups is itself lowered to nothing. Real PAC would need
an arm64e build of the whole userland (libSystem, libobjc, libc++, every framework) and a kernel with
`ptrauth_calls`: not what upstream builds, and nothing an application can observe is gained by it.

Measured on the M1 (13 November, the test copy in `/private/tmp/sfy`, nothing installed):
- The kernel execs the arm64e slice of the universal file and starts dyld in it.
- dyld then refused every library: `arm64 dylibs cannot be loaded into arm64e processes`. dyld decides
  that from the apple string `ptrauth_disabled=1`, which only a kernel built with `ptrauth_calls` sends,
  or from the main program being plain arm64.
- Fix (`libsystem-default/patches/dyld-arm64e-keys-off.patch`): dyld signs a probe value at start; where
  signing leaves it unchanged nothing in the process can sign or authenticate, and the keys count as
  disabled. Then arm64 libraries load into an arm64e program, arm64e slices and thin arm64e libraries load
  into any program (a program prefers slices of its own kind), and the "platform binaries only" rule for
  arm64e does not apply. On a kernel that really signs the probe changes and dyld behaves as before.
  arm64 programs are not affected: for them dyld already took the keys as disabled.
- With that dyld (a test copy of the binary naming `/tmp/sfy/dyld` as its dynamic linker; 13 bytes, the
  same length) the refusal is gone and dyld goes on to the first absent library.

## Faults that were in the way, in the order met (each is a fix in the system, none in Terminal)
| # | Symptom | Cause | Fix |
|---|---|---|---|
| 1 | dyld: `arm64 dylibs cannot be loaded into arm64e processes` | dyld takes the PAC keys as enabled in an arm64e program | `libsystem-default/patches/dyld-arm64e-keys-off.patch` (above) |
| 2 | 214 imports missing | section "Load surface" | |
| 3 | dyld unloads a wrapper's re-exported library, then "image not found" | dyld keeps one image per install name; a wrapper carried the name of the library it re-exports | wrappers are linked under `<path>.sfcompat` (`compat/build.sh`) |
| 4 | SIGBUS in libobjc while dyld tells it the images are mapped | current linkers put `__objc_protorefs`, `__objc_superrefs` into the read-only `__DATA_CONST` segment; dyld 852 has made it read-only by then, Apple's current dyld opens it for the runtime | `libsystem-default/patches/dyld-objc-writes-data-const.patch` |
| 5 | SIGSEGV in `swift_getObjectType`, first Swift call of `main` | upstream's arm64 Swift runtime carries Apple's arm64e isa mask (`0x00007ffffffffff8`); this libobjc is a plain arm64 build with objc4's other packed isa (`0x0000000ffffffff8`, magic bits above). Any Swift on arm64 here, not an arm64e matter | `build-swift-runtime.nix` |
| 6 | SIGSEGV formatting `%@` in nib loading | AppKit called `-stringByAppendingFormat:`, which its Foundation headers do not declare: compiled as a fixed-argument call, the method read its argument from the stack | AppKit patch 0021 |
| 7 | "Unable to load main nib file", every keyed-archive nib nil | PD's `NSKeyedUnarchiver` read only PD's own archive format, not Apple's (`$archiver`, `$top`, `$objects`, UIDs). AppKit's own nibs are NIBArchives, read by AppKit | Foundation patch (below) |
| 8 | SIGSEGV in `-[NSWindow _updateWSState]` | `strncpy` from a nil window title | 0021 |
| 9 | one missing method ended the process | objc4's `-doesNotRecognizeSelector:` ends the process; on macOS it is an exception the event loop reports. Then: the exception could not pass Foundation's forwarding trampoline (no unwind information) | Foundation patch; AppKit 0021 reports exceptions from run-loop callouts too |
| 10 | `-[NSArray boolValue]` from a binding | key paths with `@count` were not understood (no collection operators) | Foundation patch |
| 11 | SIGSEGV in `CFStringGetLength` from Terminal's `-drawRect:` | `[[NSAttributedString alloc] init]` gave the 16-byte shell `+alloc` makes, read as a CoreFoundation object | Foundation patch |
| 12 | Terminal's own assertion in every `-drawRect:` (`TTView.m:2494`) | `-[NSString cStringUsingEncoding:]` returned the string's internal pointer or NULL; Terminal asks for UTF-16 | Foundation patch |

## Load surface (what was added so that all 733 imports resolve)
Table before: `homebrew/gapscan/terminal-before.md` (214 missing). After: `terminal-after.md` (0).

| Library | Missing before | How | Real or minimal |
|---|---|---|---|
| libswiftCore and overlays | 83 (absent) | `build-swift-runtime.nix`: upstream PureDarwin's build of Swift 6.3.3 from swift.org (Apache-2.0 with runtime exception) and the open 5.4.3/5.9 overlays, at the tip, with the isa mask corrected | real |
| Foundation (Swift half) | 5 of 16 | `swift/SFBridging.swift` in the overlay: Dictionary and Int, Bool bridging | real (element by element) |
| Foundation (Objective-C) | 11 | wrapper `compat/Foundation.m` (re-exports libFoundation and libswiftFoundation, upstream's framework-compat layout): `NSFileTypeForHFSTypeCode`, `_NSDictionaryOfVariableBindings`, `NSScriptCommand`, `NSScriptClassDescription`, the object specifiers | real as data objects; nothing delivers Apple Events |
| CoreFoundation | 14 | wrapper `compat/CoreFoundation.m` (re-exports libCoreFoundation and libFoundation): `CFStringTokenizer*` on ICU's break iterators, three NSURL keys; `_CFWebServicesCopyProviderInfo` | tokenizer real (words, sentences, lines); web services: no provider |
| libSystem | 13 | `libsystem-default/patches/macos26-sdk-imports.patch`: `malloc_type_*` (the type descriptor is not used), `_os_feature_enabled_impl` (no flag files: off), `qtn_proc_*` (object real, applying answers ENOTSUP), `os_variant_is_basesystem`, `strnvis` | real except quarantine |
| AppKit | 37 | patches 0019 (tip's NSStackView, NSAccessibilityElement), 0020 (`NSMacOS26Additions`: 19 constants, accessibility helpers, gesture recognizers that keep their configuration, NSTextFinder without a find bar, NSWindowTab, private classes NSScrollerImpPair, NSFunctionRow, NSFindIndicator, NSInputManager that answer nothing and say so once, and about 60 methods) | mixed; each says in its comment what it does not do |
| CoreGraphics | 11 | wrapper `compat/CoreGraphics.c`: font smoothing getters answer what Onyx2D does (grey anti-aliasing, no LCD), CGS/CPS window-server calls answer "not implemented" | minimal; the getters belong in the framework |
| CoreText | 2 | wrapper: `CTFontDrawGlyphsWithAdvances` on `CTFontDrawGlyphs`, `CTFontShouldAntiAlias` | real, untested |
| ColorSync | 11 (absent) | `compat/ColorSync.c`: profile by name, no transform (NULL) | minimal |
| Carbon | 6 | wrapper: no Carbon events, no dead-key state (xkb composes) | minimal |
| AudioToolbox | 6 | wrapper: `AudioServices*` answer errors | minimal: silent bell |
| CoreServices | 5 | wrapper: `CreateTextToUnicodeInfo`, `ConvertFromTextToUnicode` on CFString's converters, `kUTTypeSymLink` | real |
| QuartzCore | 2 | wrapper: `kCATransition`, `kCATransitionFade` | constants |
| DataDetectorsCore, CoreAnalytics, libScreenReader | 4, 2, 0 (absent) | `compat/*.c` | by design: no detector, no analytics, no screen reader |
| ApplicationServices | 2 (absent) | the Cocoa stream's build (with ImageIO, which it re-exports), from `cocoa/build-firefox-root.nix` | SF-U's |

Wrappers: a wrapper stands at the framework's path and re-exports the real library, which the
installer keeps beside it as `<name>.pd` (for the test: `/private/tmp/sfy/pd`). CoreFoundation and
Foundation need no `.pd`: on the disk root those two paths are links to `/usr/lib/lib*.dylib`. This
is upstream's own framework-compat layout, used here so that libCoreFoundation, libFoundation and the
Cocoa stream's frameworks did not have to be rebuilt to get to `main`. Folding the additions into the
frameworks' own sources is the cleaner end state (the CoreGraphics getters need it).

Foundation (`build-foundation.nix`, `patches/foundation-macos-applications.patch`, against the tip):
a reader for Apple's keyed archives in `NSKeyedUnarchiver` (class substitution, delegate callbacks,
`-replaceObject:withObject:`, value classes built from Apple's keys, objects known before their
`-initWithCoder:` runs and never retained through it), catchable unrecognized-selector exceptions,
unwind information for `NSForwardingTrampoline`, KVC collection operators, `-mutableArrayValueForKey:`,
`-[NSAttributedString init]`, a converting `-cStringUsingEncoding:`, about 30 methods, and two
diagnostics (`NSUnrecognizedSelectorBacktrace=1`, `NSExceptionRaiseBacktrace=1`). Upstream's
`PD_OBJC_UNRECOGNIZED=continue` lists many missing methods in one run.

Reused from SF-U (nothing copied): the whole AppKit stack with patches 0001 to 0018, its NIBArchive
reader, its WindowServer with compositor frames and fractional scale (the M1 compositor answered with
server-side frames at 150%), ApplicationServices/ImageIO, the patchwork trees (`runs/cocoa/patchwork`;
a frozen `d/` = patches 0001 to 0018 was added, patches 0019 on are `d` to `b`).

## On the M1's disk and how to revert
- New files only, from the Cocoa stream's root (`cocoa/build-desktop-root.nix`): `/System/Library/Fonts/{Inter,SourceCodePro,URW}`,
  `/etc/fonts/**`, `/share/fontconfig/**`, `/System/Library/Themes/Default.plist`, the directory `/var/cache/fontconfig`.
  List: `runs/sf-terminal/m1-data-files.txt` (129 paths); revert by deleting them. Fonts are open:
  Inter 4.1 (rsms/inter release, SIL OFL 1.1, licence file beside it), URW base35 (ArtifexSoftware
  `c1510559`, AGPL-3.0 with the font exception, LICENSE and COPYING beside them), Source Code Pro (Adobe,
  SIL OFL 1.1, from nixpkgs): upstream's `nix/pkgs/base/system-fonts.nix`. Nothing from macOS.
- `/private/tmp/sfy` (scratch: the stage, Terminal.app, logs, pictures): gone at the next boot or by `rm -rf`.
- Nothing that existed was replaced. SFBOOT, the kernel, dyld, libSystem, Foundation, AppKit on the disk are as before.

## Tools
- `tools/sf_terminal.sh stage|push|run|shot|scan`: everything above from the committed recipes.
- `tools/sf-catch.c`: runs a program and reports a Mach exception from outside (registers, frames by image, optional
  memory at a register `SFCATCH_DUMP=19`, image list `SFCATCH_IMAGES=1`). Works before initializers and in arm64e processes.
- `probe/sf-nibprobe.m`: decodes a compiled nib as NSNib does and names what stands in the way.
- `homebrew/sf_gapscan.py --app`.

## Commands
```sh
python3 -B integrations/puredarwin/homebrew/sf_gapscan.py --root runs/sf-gaps/m1root --overlay runs/sf-compat/stage/new \
  --app private/terminal-app/m5-26.5.2/Terminal.app=/Applications/Utilities/Terminal.app --arch arm64e --name terminal
T=integrations/puredarwin/terminal-app/tools/sf_terminal.sh
$T stage && $T scan && $T push          # about 3 minutes when the nix builds are cached (Swift runtime: 5 minutes)
$T run 25 PD_WS_TRACE=1                 # a window for 25 s on the target's desktop; KEEP=1 leaves it
$T shot name                            # runs/sf-terminal/shots/name-N.png: what the windows presented
```
Awaiting the M1: nothing. Awaiting QEMU: the pass for dyld, libSystem and libFoundation (fresh boot
included), then `sf-lib-install.sh`-style guarded replacement; then the unmodified bundle at
`/Applications/Utilities/Terminal.app`.
