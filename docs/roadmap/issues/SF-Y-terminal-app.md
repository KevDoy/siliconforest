# SF-Y: Apple's Terminal.app on SiliconForest (GitHub issue 52)

## Goal
The unmodified Terminal.app binary from the M1's own macOS launches and works on the SiliconForest
desktop: the demonstration that macOS applications run here. The binary is never committed; every
step that makes it run is part of SiliconForest. Phases: 1 measure (done by the coordinator),
2 load (dyld reaches `main`), 3 launch (a window on the compositor), 4 a working shell in it.

Test copy: `private/terminal-app/m5-26.5.2/Terminal.app` (macOS 26.5.2, from the controller Mac;
git-ignored). The final binary comes from the M1's macOS 27.0: nothing below may depend on one build.

## State (13 November 2026, night)
**On the M1, booted from its disk: the unmodified Terminal.app (the macOS 26.5.2 copy, 129 files
hash-equal to the copy here, at `/Applications/Utilities/Terminal.app`) is started with
`open -a Terminal`, opens an 80x24 window in the compositor's frame, draws with the font of its own
bundle, runs the user's shell on a pty and takes what is typed.** Seen after a fresh boot
(`runs/sf-terminal/shots/v1.png`: launched over SSH; `w1.png`: launched by typing the command into
the desktop's own terminal). The five files every program loads (dyld, libSystem, libFoundation and
the two framework paths) were replaced on the M1 by the guarded procedure after a QEMU pass with a
fresh boot of its result; the two regression checks give what they gave before. Everything is
built by recipes; the libSystem recipe that every root uses now carries the dyld and libSystem
changes.

| Phase | State | Evidence |
|---|---|---|
| 2 load | done on the M1 | scanner: 733 imports, 0 missing (`homebrew/gapscan/terminal-after.md`); dyld binds all of them and runs the initializers |
| 3 launch | done on the M1 | an 80x24 window in the compositor's frame, title "Terminal — sh — 80×24" (`runs/sf-terminal/shots/n1.png`, a capture of the screen) |
| 4 shell | done on the M1 for what the table below does not list | text, colours and bold drawn; typing, Return, Backspace, arrows, Ctrl-C, Ctrl-D, Tab; scrollback (PageUp), window resize (the shell sees 100x31), selection by dragging, Command-C and Command-V: `runs/sf-terminal/shots/{m2,s6,r6,c1,c4,f1}-crop.png` |

Pictures: `runs/sf-terminal/shots/` (not committed). Those named above are captures of the whole
screen as the compositor rendered it (its inject module, below); the older `t*`, `m1-window*` ones
are what the application handed to the compositor (`PD_WS_DUMP_DIR`). Nobody looked at the panel.
Input in every test is synthetic: key and pointer records the compositor generates on request at
the layer of its HID device nodes and labels so. No physical keyboard or trackpad was used.

### What was wrong with text and keys (all in the system, none in Terminal)
| Symptom | Cause | Fix |
|---|---|---|
| White window, cursor cut off at the top | Terminal sizes its window from a key-value observation of `activePane.view.contentSize`. Foundation's KVO knew neither key paths nor setters called directly, so the window kept the nib's 524x327 while the view laid out 24 rows of 15 points: rows 0 and 1 (where the text was) lay above the top edge | Foundation: KVO with key paths, dependent key paths and setters that notify (the class's setter wrapped by a thunk; no per-instance subclass) |
| A cell 12 points wide | Terminal asks for the font "SF Mono Terminal" (its own, in its bundle); CoreText resolved a name no installed face carries through fontconfig's default: DejaVu Sans, proportional. A terminal measures its cell from the advance of "W" | CoreText (0023): Menlo, Monaco, SF Mono and any family whose name says fixed-pitch resolve to an installed fixed-pitch face; AppKit (0024) asks CoreText for unknown names; AppKit (0021) registers the fonts an application carries (`ATSApplicationFontsPath`), so Terminal draws with its own face from its own bundle |
| The program ended in the bindings of the preferences window once notifications arrived | a selection index beyond the content asserted in CoreFoundation; a binding to a key a control lacks raised through a dispatch callout | AppKit (0024): such a selection selects nothing; a failing binding is reported once |
| Title "80\U00D724" | the title's parts come from `Localizable.loctable` (one property list for all localizations); only `.strings` files were read | Foundation: NSBundle reads localization tables |
| Scrolling back raised | `-[NSView scrollRect:by:]` ends in `-copyBitsInRect:toPoint:gState:`, defined only for Onyx2D's abstract context | Onyx2D (0026) |
| Command-V pasted nothing | Terminal asks the pasteboard for `_sourceSigningIdentifier` (private) before it pastes | AppKit (0020): answers nil (nothing records who wrote a pasteboard) |
| Keys | nothing: `-interpretKeyEvents:` already reached `-insertText:replacementRange:` and the command selectors | - |

### What stands between this and "perfectly working" (in the order a person meets it)
| Item | What is known | Estimate |
|---|---|---|
| Launch takes 2.3 s to the shell | measured from `open` to the shell's process on the installed bundle. It was about 7 s: Terminal loads its preferences window at launch to import its default profiles, and decoding that nib's tab view ran the Cocoa stream's layout solver 388 times over the whole tree (4.8 s on the main thread, measured with `sf-trace`). AppKit 0027 solves once per resize cascade. Real Terminal on this hardware is under a second | 1 to 2 days for the rest (the solver itself, nib decoding) |
| No menu bar | the main menu loads from the nib and its key equivalents work (Command-C, Command-V, and the rest by the same path); nothing shows it. SF-U's proposal (a strip in the compositor's frame fed by a small Wayland protocol) is not built | SF-U item, about a week |
| Root's shell is `/bin/sh` | Terminal starts the user's shell from the password file; this root's `/bin/sh` (zsh in sh emulation) has no line editing, so arrows echo `^[[A` until `exec zsh`. Not Terminal's: a login user with `/bin/zsh` | a decision about users, not code |
| Resize handle is 2 pixels | the compositor's frame has a 2-pixel border as its resize zone for this window; resizing itself works and Terminal reflows | compositor |
| Preferences window | Command-comma opens it and its General pane is laid out and readable (`shots/p4-crop.png`). It came up empty until two faults were fixed (a nib without a maximum size decoded as "no larger than 0 by 0"; fonts archived without a size were drawn at size 0). Open: no toolbar is shown, so the other panes (Profiles, Encodings) cannot be chosen; labels are cut off at the right ("New window with profi…": the system font here is wider than the nib was laid out for); bindings to `NSMatrix.selectedTag` and `NSPopUpButton.content`, `.contentObjects`, `.contentValues` have no such keys; `data source ... does not respond to numberOfRowsInTableView:`. Changing a setting was not tried | 1 week |
| Symbol images, `Assets.car` | `Could not find image named 'gearshape'` and others: SF Symbols and the CoreUI catalogue are not read; AppKit's `NSColorPanel.nib` is missing from its resources | 1 week for a `.car` reader, symbols need an open symbol font |
| Scroller look | Terminal drives its scroller through `NSScrollerImpPair` (private), which answers nothing; the knob is drawn by AppKit's own scroller and follows the position, in the Cocotron look | 3 days |
| Tabs, new windows, find, bookmarks, Look Up, Quick Look, help | not tried; 213 selectors no class implements (static count), 130 of them delegate callbacks Terminal answers itself | as they are met |
| Sound | `AudioServices*` answer errors: the bell is silent | 2 days over the sound stream's server |
| Mouse reporting, drag and drop, services, AppleScript | not tried | unknown |
| Private frameworks | DataDetectorsCore (no links found in output), CoreAnalytics (nothing collected), libScreenReader (empty): by design | - |
| Swift | the 5.4.3 overlay subset plus `swift/SFBridging.swift`; nothing of Terminal's Swift code beyond `swift_getObjectType` at launch was seen running | unknown |
| A burst of keys once stalled for 0.6 s | seen once with the tracer loaded (25 keys per second otherwise arrive in step); not reproduced without it | not established |
| The M1's own Terminal (macOS 27.0) | pending: macOS was not running, so the bundle could not be fetched. Nothing here names a symbol address or an offset of the 26.5.2 build; rescan with `sf_terminal.sh scan` once it is there | rescan |

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
| CoreGraphics | 11 | in the framework (cocoa patch 0025, `CGPrivateAdditions.m`): font smoothing getters answer what Onyx2D does (grey anti-aliasing, no LCD; not what was last set), CGS/CPS window-server calls answer "not implemented" | minimal |
| CoreText | 2 | in the framework (0023): `CTFontDrawGlyphsWithAdvances` on `CTFontDrawGlyphs`, `CTFontShouldAntiAlias` | real: every glyph Terminal draws goes through the first |
| ColorSync | 11 (absent) | `compat/ColorSync.c`: profile by name, no transform (NULL) | minimal |
| Carbon | 6 | wrapper: no Carbon events, no dead-key state (xkb composes) | minimal |
| AudioToolbox | 6 | wrapper: `AudioServices*` answer errors | minimal: silent bell |
| CoreServices | 5 | wrapper: `CreateTextToUnicodeInfo`, `ConvertFromTextToUnicode` on CFString's converters, `kUTTypeSymLink` | real |
| QuartzCore | 2 | wrapper: `kCATransition`, `kCATransitionFade` | constants |
| DataDetectorsCore, CoreAnalytics, libScreenReader | 4, 2, 0 (absent) | `compat/*.c` | by design: no detector, no analytics, no screen reader |
| ApplicationServices | 2 (absent) | the Cocoa stream's build (with ImageIO, which it re-exports), from `cocoa/build-firefox-root.nix` | SF-U's |

Wrappers (`build-compat.nix`, built against recipes, not against a copy of a running root): a wrapper
stands at the framework's path and re-exports the real library, which stays beside it as `<name>.pd`
(QuartzCore, Carbon, CoreServices, AudioToolbox). CoreFoundation and Foundation need no `.pd`: at
those two paths the root has links to `/usr/lib/lib*.dylib`, and the wrappers re-export those
libraries (Foundation's also the Swift overlay, whose names Apple's programs bind to Foundation; on
macOS the collection classes live in CoreFoundation, so that wrapper re-exports libFoundation too).
This is upstream's own framework-compat layout. Folded into the frameworks' own sources so far:
CoreGraphics and CoreText (above). Still wrappers because their frameworks come from four different
recipes: QuartzCore (two constants), Carbon, CoreServices, AudioToolbox.

Foundation (`build-foundation.nix`, `patches/foundation-macos-applications.patch`, against the tip):
a reader for Apple's keyed archives in `NSKeyedUnarchiver` (class substitution, delegate callbacks,
`-replaceObject:withObject:`, value classes built from Apple's keys, objects known before their
`-initWithCoder:` runs and never retained through it), catchable unrecognized-selector exceptions,
unwind information for `NSForwardingTrampoline`, KVC collection operators, `-mutableArrayValueForKey:`,
key-value observing with key paths, dependent key paths and notifying setters (the setter of the
class is replaced by a thunk that brackets the original with will/did, for all instances; arm64
only; no per-instance subclass, so `object_getClass` of an observed object is its class),
localization tables (`.loctable`), `-[NSAttributedString init]`, a converting
`-cStringUsingEncoding:`, about 30 methods, and two diagnostics (`NSUnrecognizedSelectorBacktrace=1`,
`NSExceptionRaiseBacktrace=1`). Upstream's `PD_OBJC_UNRECOGNIZED=continue` lists many missing
methods in one run.

Reused from SF-U (nothing copied): the whole AppKit stack with patches 0001 to 0018, its NIBArchive
reader, its WindowServer with compositor frames and fractional scale (the M1 compositor answered with
server-side frames at 150%), its layout solver, popups and key equivalents, ApplicationServices and
ImageIO, the patchwork trees (`runs/cocoa/patchwork`; a frozen `d/` = patches 0001 to 0018 was added,
patches 0019 on are `d` to `b`). Reused from the macOS-compatibility stream (issue 45): its guarded
library installer, its QEMU rehearsal and its check, with this job's stage (`SF_COMPAT_STAGE`).
Changed in SF-U's stack by this job: its document has the list ("what the Terminal.app stream changed").

## The system side as recipes
`terminal-app/build-macos-apps-root.nix` is one tree rooted at `/`: everything a root needs beyond
its base so that Terminal runs, every file from a recipe. `sf_terminal.sh stage` tries that tree
from a scratch prefix; `install-stage` splits it for installing.

| Part | Recipe | In the root's own recipe? |
|---|---|---|
| `/usr/lib/dyld`, `/usr/lib/libSystem.B.dylib` | `libsystem-default/build-libsystem.nix` | yes: the default libSystem recipe now has the two dyld patches and the macOS 26 SDK imports (every RAM root and image built from it gets them) |
| `/usr/lib/libFoundation.dylib` | `terminal-app/build-foundation.nix` (upstream's Foundation at the tip + one patch) | laid over the base by the tree; upstream's Foundation recipe is not ours to change |
| `/usr/lib/swift/*` | `terminal-app/build-swift-runtime.nix` | new in the tree |
| AppKit, CoreText, CoreGraphics, Onyx2D, libWindowServer | `cocoa/patched.nix` (patches 0001 to 0027) | yes: the Cocoa stream's root recipes (`build-desktop-root.nix`, `build-firefox-root.nix`) take all five from `patched.nix`, and `macos-compat/build-appkit.nix` is a selection of that root. That stream's own checks were not re-run on 0019 to 0027 |
| the wrappers (CoreFoundation, Foundation, QuartzCore, Carbon, CoreServices, AudioToolbox), ColorSync, DataDetectorsCore, CoreAnalytics, libScreenReader | `terminal-app/build-compat.nix` | new; links against recipes (libSystem, libobjc, CoreFoundation, Foundation, ICU, the Swift runtime, the Cocoa root, `macos-compat/build-coreservices.nix`, the sound stream's AudioToolbox), not against a copy of a running root |
| ApplicationServices, ImageIO | `cocoa/build-firefox-root.nix` | the Cocoa stream's |
| `/usr/bin/open` | `terminal-app/system/open.c` | new |
| the installer's probes (`/usr/share/sf-test/sf-*-probe`) | `terminal-app/probe/` | new |

`macos-compat/build-compat-root.nix` ("what a SiliconForest root gets for macOS programs") lays the
tree over its AppKit and Perl parts. Not wired: the tools that make a disk root from it lay only
paths the base lacks, so on a root made anew libFoundation, the two framework paths and the wrapped
frameworks are still the base's until `sf_terminal.sh install core` and `install direct` have run
on it (the image builder of the storage stream is not this job's to change).

## Installing
Five files are loaded by every program: dyld, libSystem, libFoundation, and the two framework paths
`CoreFoundation.framework/Versions/A/CoreFoundation` and `Foundation.framework/Versions/C/Foundation`
(links to `/usr/lib` on the root; the wrappers replace the links, and programs built on macOS name
exactly those paths). They go through:
1. `sf_terminal.sh qemu <name>` then `qemu <name> second`: the M1's root image in QEMU
   (`macos-compat/tools/sf_compat_qemu.sh` with this job's stage), the replacements done there as on
   the M1, the macOS-compatibility check, then a fresh boot of the root that leaves.
2. On the M1, `sf_terminal.sh install core`: for each library `macos-compat/sf-lib-install.sh`
   (the new file written beside the installed one, hashed there before anything loads it, the probe
   and ls, sh, node, claude, python3, bash, git, vim, nvim, tmux run with it through `DYLD_*`, the file
   in use kept as `.old`, renamed, the same programs as new processes; a failed confirmation renames
   back). An installed path that is a link is kept as a link.
3. `/usr/lib/dyld` last, by `guest/sf-dyld-install.py`: no `DYLD_*` variable reaches the linker,
   so programs are run as copies that name the new file (`/usr/lib/dyl2`, the same length) as their
   linker; the swap and its way back are `link(2)`/`rename(2)` inside one Python process, so a linker
   that does not start programs is undone without starting one.
Everything else (`install new`, `install direct`): `guest/sf-apps-install.sh` hashes each file on
the target, keeps an existing file (`<path>.pd` where the new file is its wrapper, `<path>.old-sfy`
otherwise) and records what it did in `/private/var/db/sf-macos-apps.installed`.

### What was run (13 and 14 November)
| Step | Result | Where |
|---|---|---|
| QEMU, the M1's root image with macos-compat's additions | libSystem, libFoundation, both wrappers, then dyld replaced as on the M1; macos-compat check 18 of 18; `QDDESK_PASS`, `COMPAT_VERDICT failed=0` | `runs/m1-storage/qemu-dr/ty4/serial.log` |
| QEMU, a fresh boot of the root that run left | the five probes and node, claude, python3, bash, git, vim, nvim, tmux as new processes, the check again: `QDDESK_PASS`, 33 verdicts ok, `failed=0` | `runs/m1-storage/qemu-dr/ty4-2/serial.log` |
| (earlier rehearsals) | `ty1`: a root without macos-compat's additions (its check failed for that reason; the five replacements passed); `ty2`, `ty3`: stopped, the stage was rebuilt | same directory |
| M1: `install new`, `install app` | 35 files and 11 links the system lacked; the bundle, 129 files equal | `runs/sf-terminal/m1-install/` |
| M1: `install core` | libSystem (the file of before is `.old3`), libFoundation (`.old`), CoreFoundation and Foundation wrappers (the links kept as `.old`), dyld (`.old`): each `SFLIB 3 installed` / `SFDYLD 3 installed` with the ten programs at the statuses they had | `runs/sf-terminal/m1-install/*.txt` |
| M1: `install direct` | AppKit, CoreText, CoreGraphics, Onyx2D, libWindowServer replaced (kept as `.old-sfy`), QuartzCore, Carbon, CoreServices, AudioToolbox wrapped (the library kept as `.pd`), `/usr/bin/open` new | `direct.txt` |
| M1: regression, before and after | `macos-compat/tools/sf_compat_push.sh check`: 18 of 18 ok both times; `sf-claude-check.sh`: the same statuses and the same two TUI screens | `runs/sf-terminal/regress/` |
| M1: restart, then `open -a Terminal` | the unmodified bundle runs, commands typed into it run | `shots/v1.png`, `w1.png` |

The dyld and libSystem that were rehearsed and installed are byte for byte what
`libsystem-default/build-libsystem.nix` builds (sha256 `472fd9a3…` and `dfc854f1…`); libFoundation
`ac8a048b…`, the wrappers `525419ad…` and `c4d12d0d…`. Not replaced: `/usr/lib/system/libdyld.dylib`
(the recipe's differs from the installed one in 539 bytes of its symbol table; the installed one was
used in every test) and libobjc, libCoreFoundation (unchanged).

## On the M1's disk and how to revert
- The five core files: `ln <path>.old <path>.back && mv -f <path>.back <path>` each (libSystem's is
  `.old3`; for the two framework paths the kept file is a link: `ln -s "$(readlink <path>.old)"
  <path>.back && mv -f <path>.back <path>`). dyld first if all are to go: the old dyld starts every
  program with any of the libraries.
- Everything `install new` and `install direct` put there: `sf_terminal.sh revert direct`
  (`/private/var/db/sf-macos-apps.installed` is the list; new files are removed, `.old-sfy` and
  `.pd` files renamed back).
- `/Applications/Utilities/Terminal.app`: remove it. `/var/root/Library/Preferences/com.apple.Terminal.plist`
  and `/var/root/Library/Logs/Terminal.log` are what running it leaves.
- The session script: `/usr/libexec/sf-session.old` is the one of before. The trial compositor is
  `/usr/bin/chonkstep-darwin-desktop.next` (the installed compositor was not touched); the marks
  `/private/var/db/desktop-inject` (the inject directory) and `desktop-next-tried`: remove all three
  to be rid of it. With the mark renamed to `desktop-next-try` the next boot runs the trial build once.
- From the first day, from the Cocoa stream's root: `/System/Library/Fonts/{Inter,SourceCodePro,URW}`,
  `/etc/fonts/**`, `/share/fontconfig/**`, `/System/Library/Themes/Default.plist`,
  `/var/cache/fontconfig` (`runs/sf-terminal/m1-data-files.txt`, 129 paths). Fonts are open:
  Inter 4.1 (SIL OFL 1.1), URW base35 (AGPL-3.0 with the font exception), Source Code Pro (SIL OFL
  1.1). Nothing from macOS but Terminal.app itself; the font Terminal draws with is inside its bundle
  and is read from there (`PDApplicationFonts=NO` in the environment or its defaults turns that off).
- `/private/var/db/sfy` (the scratch prefix, `/private/tmp/sfy` links to it after `ln -sfn`): remove.

## Launching
`open -a Terminal` or `open /Applications/Utilities/Terminal.app` (`/usr/bin/open`, new: the bundle's
executable in a session of its own with the desktop's environment, output in
`~/Library/Logs/<name>.log`), typed in the desktop's terminal or over SSH. The desktop menu has no
entry for applications: its rows are the compositor's own (Terminal, Theme, Wallpaper, Display,
Wi-Fi, Sound, End Session) and its launch path admits only its own terminal; an "Applications" row
that runs `open` is the natural next step and is not built.

## Tools
- `tools/sf_terminal.sh`: stage, push, run, shot, scan (scratch prefix); install-stage, qemu,
  install, launch, revert.
- `tools/sf_inject.sh`, `tools/sf_keys.py`: typed text, keys, chords, pointer and captures of the
  screen through the compositor's inject module (`compositor/wm-darwin-inject.patch`, against the
  source of the compositor the disk has; built with `chonkstep-darwin/tools/build_desktop_binaries.sh`).
  `m1-storage/system/sf-session` runs `/usr/bin/chonkstep-darwin-desktop.next` for one boot when
  `/private/var/db/desktop-next-try` exists and gives the compositor its inject directory when
  `/private/var/db/desktop-inject` exists.
- `probe/sf-trace.c` (+ `sf-trace-thunks.S`): the methods of chosen classes as a program calls them,
  with arguments, results, objects named, and times (`DYLD_INSERT_LIBRARIES`, `SFTRACE=...`). How the
  causes above were found.
- `tools/sf_methods.py`: where an application's methods are and what each calls (reads the binary).
- `tools/sf-catch.c`: runs a program and reports a Mach exception from outside.
- `probe/sf-nibprobe.m`: decodes a compiled nib as NSNib does and names what stands in the way.
- `homebrew/sf_gapscan.py --app`.

## Commands
```sh
T=integrations/puredarwin/terminal-app/tools/sf_terminal.sh
$T stage && $T scan && $T push          # the tree, tried from /private/tmp/sfy (nothing installed)
$T install-stage                        # runs/sf-terminal/install
$T qemu NAME && $T qemu NAME second     # the rehearsal and the fresh boot of its result (25 + 20 minutes)
$T install new; $T install app; $T install core; $T install direct
$T launch 20                            # or on the target: open -a Terminal
I=integrations/puredarwin/terminal-app/tools/sf_inject.sh
$I 'text:echo hello\n' key:up chord:ctrl+c wait:500 capture:name     # runs/sf-terminal/shots/name.png
```
Pending: the M1's own Terminal (macOS 27.0), which needs macOS running to fetch.
