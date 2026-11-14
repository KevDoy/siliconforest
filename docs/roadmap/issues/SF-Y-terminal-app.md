# SF-Y: Apple's Terminal.app on SiliconForest (GitHub issue 52)

## Goal
The unmodified Terminal.app binary from the M1's own macOS launches and works on the SiliconForest
desktop: the demonstration that macOS applications run here. The binary is never committed; every
step that makes it run is part of SiliconForest. Phases: 1 measure (done by the coordinator),
2 load (dyld reaches `main`), 3 launch (a window on the compositor), 4 a working shell in it.

Test copy: `private/terminal-app/m5-26.5.2/Terminal.app` (macOS 26.5.2, from the controller Mac;
git-ignored). The final binary comes from the M1's macOS 27.0: nothing below may depend on one build.

## State (14 November 2026)
**On the M1, booted from its disk: the unmodified Terminal.app (the macOS 26.5.2 copy, 129 files
hash-equal to the copy here, at `/Applications/Utilities/Terminal.app`) is started from the desktop
menu (Applications > Terminal) or with `open -a Terminal`. It shows its menu bar as the first row
inside the window's frame, opens windows with its twelve built-in profiles (the default is Clear
Dark, as on macOS 26), runs root's zsh through `/usr/bin/login`, and ends when its last window is
closed.** Everything below was seen in captures of the screen as the compositor rendered it
(`runs/sf-terminal/shots/`, not committed). Later on 14 November: output on the screen about 80 ms
after it reaches the pty, launch about 1 s, Command-Q quits, tabs (one toplevel per tab group), bold
and italic faces (table below). Input in every test is synthetic (the compositor's
inject module, a test instrument: `terminal-app/compositor/README.md`). Nobody looked at the panel
and no physical keyboard or trackpad was used.

**Fourth round (night of 14 November): the section "Fourth round" below is the current state;
where it and the rows here differ, it is right.**

**14 November, third round (the rows below it are the earlier rounds').** In Terminal on the M1, with synthetic input: rows a program did not redraw no longer
stay grey (tmux, a click in vim: `fin-tmux-vim`); tmux finds `tmux-256color` (the whole ncurses
database is in the root); 24-bit colour (`COLORTERM=truecolor`, `tc3`); its questions are macOS
alerts with Terminal's icon, centred on their window, with Return and Escape (`al10`, `al11`);
Command-F find bar with Find Next and Previous (`fd1`, `fd7`..`fd9`); Preferences lists the twelve
profiles, its labels fit, its tab view has the macOS 26 look and a changed setting is in the
preferences file after a relaunch (`pf8`, `ps4`); Chinese, Japanese, Korean and emoji
(monochrome) draw at the right widths (`wc1`); `open x.command` runs the file in Terminal (`op1`);
the Window menu shows Shift-Command-[ (`wm2`); split pane (`sp1`). Measurements below ("Performance,
third round"). The desktop's own terminal no longer leaves a white block where it was clicked
(`ct-before`, `ct-after`).

| What | State | Evidence (`runs/sf-terminal/shots/`) |
|---|---|---|
| Menu bar | done: every titled window that is not a panel carries the main menu as a strip (AppKit 0028, one mechanism for all Mac applications). Pointer: press a title, move along the bar, choose. Keyboard: Control-F2, arrows, Return, Escape | `mb`, `mc2`..`mc4` (pointer), `md1`..`md6` (keyboard; Shell > New Window chosen by Return opened a window), `qt1`, `qt2` (Terminal > Quit Terminal ended the program and both shells) |
| Windows | Command-N, Command-W, closing the last window (the program ends, no shell is left), Terminal > Quit. Not right: Command-Q (below), Command-T | `cw1`..`cw3`, `pr6` |
| Profiles | the twelve built-in profiles are imported at first launch; Shell > New Window > <profile> opens a window with that look (Ocean: blue); the default window is Clear Dark, 120x30 | `pr4`, `pr5`, `pr6` |
| Preferences | Command-comma; the toolbar shows General, Profiles, Window Groups, Encodings and switches panes; the pop-ups and checkboxes show the stored values (bindings work since key-value coding finds AppKit's `_key` accessors); the window grows for its toolbar; titles that were cut are drawn up to an eighth narrower instead. Open: below | `ph2` (General), `ph3` (Profiles: font, colour wells, cursor) |
| Desktop | the desktop menu has Applications (one row per bundle in `/Applications`, `/Applications/Utilities`) | `ap1`..`ap3` |
| Shell | `login -pf root` (ours) starts root's shell as a login shell; root's shell is zsh (`/etc/passwd`, `/etc/master.passwd`); `TERM=xterm-256color`, `TERM_PROGRAM=Apple_Terminal` | `pr4`, `th1` |
| In the terminal | 256 colours, italic, underline, reverse, dim; htop full screen; text selection, paste, scrollback, resize as before | `tc2`, `th2` |

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
| Launch | done for what was cheap: exec to the window with its shell 0.97 to 1.06 s (was 3.4 s, measured the same way: `tools/sf-latency.py launch`; the 5 s of before was another measure). Causes found with the profiler (`probe/sf-prof.c`): the five drawing frameworks compiled without `-O`; the application's fonts scanned three times at each launch (now their directory, cached by fontconfig; the first launch after an install is 2.4 s); a fontconfig match, a file read and a FreeType face for every NSFont made by name; a font made of every installed face to find one character; the menu archived for a menu bar that does not exist. Left: nib decoding (about 0.3 s), the text system for the menu strip | - |
| Output, typing, menus | done: 40 lines written to the pty are committed 66 to 72 ms later and on the screen (the compositor's release of the frame before) at about 80 ms; 33 ms of that is Terminal's own timer (it draws at most 30 times a second), drawing a screenful about 20 ms. Typed Return to output in a capture of the screen: 100 to 150 ms (zsh starting `ls` included; before: more than 300 ms, less than 1 s). Typing echo, key to commit: 15 ms (was 39). A menu, click to its popup: about 150 ms (was about 820 ms by this measure). Scrolling with the wheel: a frame every 50 ms. Maximize (the frame's button): the first frame at the new size (2556x1544 pixels) 218 ms after the click, in a second try 137 ms, of which the main thread drew about 70 ms (profile: no single cause; the menu strip about 18 ms) The 1.9 to 3.4 s of before were partly the capture instrument (each capture holds the compositor about 0.5 s) and a WindowServer that dropped a flush when every frame was busy | - |
| Command-Q | done: the compositor passes Command-Q to a Mac application (patch 5); Terminal's own Quit then needed `login` to stay the shell's parent and `+[NSCountedSet setWithCapacity:]`. Two idle windows: Command-Q ends the program and both shells; with a process running: the alert "Do you want to terminate running processes", Return ends everything (`cq4`). Third round: the alert is laid out as on macOS 26 (AppKit 0044: NSBeginAlertSheet and NSRunAlertPanel are NSAlert; 420 points, Terminal's icon, wrapped text, buttons sized to their titles, Return, Escape), has no title bar (0041) and is centred on its window by the compositor (WindowServer 0043, `xdg_toplevel.set_parent`): `al10`..`al13` | - |
| Tabs | done (AppKit 0039, WindowServer 0036): a tab group is one toplevel; Command-T, Command-Shift-[ and ], a click on a tab, + (Terminal's `newWindowForTab:`), a tab's x, Command-W, Window > Move Tab to New Window, Merge All Windows (`tb1`, `tb6`, `tb7`, `tc3`..`tc5`, `mv1`, `mg1`, `mg2`). Third round: the Window menu shows Shift-Command-[ and ] (AppKit 0046, `wm2`). Not done: dragging tabs to reorder or out of the window, Show All Tabs. The bar's light look in a dark profile: Terminal sets no appearance on its windows (it asks only `bestMatchFromAppearancesWithNames:` while drawing), so the bar follows the system appearance, light here; whether macOS draws it dark in light mode was not checked | dragging: 1 day |
| Preferences, rest | third round: the profile list shows the twelve profiles (every table column width and row height of a macOS nib was read as 0: AppKit 0046, the NIBArchive reader's `-decodeFloatForKey:` returned a double where NSCoder declares a float; then the list drew into an image's cache window: 0041), labels fit (the same float fix; 0046 narrows by up to an eighth what still clips), the tab view has the macOS 26 look (0050), a checkbox changes the profile and the change is in `com.apple.Terminal.plist` after a relaunch (0047: a control's bound value goes to the model when it sends its action; the selection proxy sets each selected object): `pf8`, `ps3`, `ps4`. "Antialias text" is disabled as on macOS (bound to `fontAllowsDisableAntialias`). Open: the profile thumbnails are grey frames (Terminal draws a TTView into an image; only Red Sands shows its top row: cause not established); toolbar icons blank (symbol images); bindings `enabled2` and `NSMenuItem.value` not looked at; Window Groups and Encodings panes not looked at; colour wells not tried (`NSColorPanel.nib` is missing) | thumbnails 1 day; the rest 3 days |
| True colour | done (third round): libSystem reads feature flags (`libsystem-default/patches/feature-flags.patch`, Finch's reader) and the root has `/System/Library/FeatureFlags/Domain/Terminal.plist` (DirectColor FeatureComplete, macOS 26.5's value). Terminal asks no other flag (one call of `_os_feature_enabled_impl`, in `-[TTShell init...]`). Rehearsed in QEMU (`ty7`, `ty7-2`), installed by the guarded installer; `COLORTERM=truecolor` and 64-step gradients without any `DYLD_*` (`tc3`) | - |
| Bold, italic | done (`at1` before, `at3` after): bold was drawn in the regular face (`-convertFont:toHaveTrait:` took the first face with the trait, the variable font's Heavy instance, and CoreText drew a named instance from the file's default instance), italic in Light Italic. Strike-through (SGR 9), double underline (21) and overline (53) are not drawn: Terminal keeps no such cell attribute, as on macOS. Underline, dim, reverse, 256 colours: drawn. Cursor shapes (DECSCUSR block, underline, bar) and blinking: drawn (`cu1`..`cu5`) | - |
| Wide characters | done (third round): `build-fonts-extra.nix` lays Noto Sans Mono CJK and Noto Emoji (monochrome; OFL 1.1) into `/System/Library/Fonts`; CoreText's fallback finds them. Chinese, Japanese, Korean at double width, emoji, box drawing and block elements, combining marks, powerline glyphs; no ligatures (`wc1`). Colour emoji: not drawn (Noto Color Emoji is CBDT bitmaps; the drawing path draws outlines) | colour emoji: 2 days |
| First key after a click | not reproduced: six times a click into the window followed by typing at once, every character arrived (synthetic input) | - |
| Symbol images, `Assets.car` | `Could not find image named 'gearshape'` and others: SF Symbols and the CoreUI catalogue are not read; AppKit's `NSColorPanel.nib` is missing. Not worked on in the third round. The application icon is read now (its `.icns`: Onyx2D 0042, PNG entries) | 1 week for a `.car` reader (Finch's CoreUI is the reference), symbols need an open symbol font |
| Scroller look | Terminal drives its scroller through `NSScrollerImpPair` (private), which answers nothing; the knob is drawn by AppKit's own scroller, in the Cocotron look | 3 days |
| Bell | `NSBeep()` and `AudioServices*` make no sound (Terminal logs "Error ... getting system alert sound" at each bell); Terminal's own visual bell was not tried. Not worked on in the third round | 2 days over the sound stream's server |
| Root's home in new windows | none: `login` starts in the home directory | - |
| Resize handle is 2 pixels | the compositor's frame has a 2-pixel border as its resize zone | compositor |
| Full-screen programs | done (third round). vim, nvim, htop, less, tmux and `claude` (its first-run TUI) draw their screens (`fv1`..`fv3`, `cl2`, `fin-tmux-vim`); mouse reporting works. The grey rows: Onyx2D clips to one rectangle, so a list of dirty rects became their bounding box, which the window frame filled with its grey while Terminal filled only the rects `-getRectsBeingDrawn:` listed. AppKit 0040: that method answers the rect drawn; dirty rects far apart are drawn one at a time; dirty rects are whole device pixels (at 150% whole points left one-pixel seams). AppKit 0041: windows start opaque, so Terminal's `-setOpaque:NO` for its translucent profiles takes effect (the window is now clear behind the text, as on macOS: windows behind show through faintly, unblurred). tmux: the whole ncurses database (`build-terminfo.nix`). Command-F: done (row "Find") | - |
| Find | done (third round, AppKit 0048, Finch's NSTextFinder): Command-F shows the bar, Return finds ("6 found"), Command-G and Command-Shift-G next and previous, Escape closes it (`fd7`..`fd9`). Use Selection for Find (Command-E) set a one-character search string after a synthetic double-click (`fd10`): not right, cause not established | 1 day |
| Documents | third round: `/usr/bin/open` hands documents over as `-NSOpen` (AppKit gives them to `application:openFile:`) and finds the application by the document types bundles declare: `open x.command` runs it in Terminal (`op1`); `open -a Terminal <folder>` opens a window, but in the folder's parent (`op2`: cause not established). Each `open` starts a new instance (no Apple Events) | folder: half a day |
| Other menus | third round, synthetic input: View > Split Pane (Command-D) splits (`sp1`); Shell > New Command opens its window (`nc1`; its layout is cramped); session restore: a relaunch restored a window with its text (`g2`). Print (Command-P): no panel appeared (`pr1`) | - |
| Not tried | drag and drop, services, Secure Keyboard Entry, New Remote Connection, marks navigation, full screen, AppleScript, Look Up, help (`-[NSHelpManager openHelpAnchor:inBook:]` is not implemented) | as they are met |
| Private frameworks | DataDetectorsCore, CoreAnalytics, libScreenReader: by design | - |
| The M1's own Terminal (macOS 27.0) | pending: macOS was not running. Rescan with `sf_terminal.sh scan` once it is there | rescan |

### Faults found on 14 November (all in the system, none in Terminal)
| Symptom | Cause | Fix |
|---|---|---|
| No menu was shown | the desktop has no menu bar process | AppKit 0028: the main menu as a strip of each window; needs `xdg_popup` in the compositor (`compositor/wm-darwin-popups.patch`, the Cocoa stream's commits, now in the M1's compositor) |
| Terminal started `sh` although the user database said zsh | Terminal runs `/usr/bin/login -pf <user>`; there was no `login` | `system/login.c` |
| Closing a window ended the program | `-[NSTabView removeTabViewItem:]` compared an `int` with `NSNotFound` and selected item -1 | AppKit 0029 |
| Closing the last window crashed in `CFRelease` | `-[NSAttributedString init]` replaced an application's subclass instance by a CoreFoundation string, into which the application then wrote its instance variables; `-retain`/`-release` of a subclass went to CoreFoundation | Foundation |
| Bindings of scalar properties raised | key-value coding reached only object instance variables | Foundation: scalars and structs in place |
| No binding of a pop-up button, matrix or tab view found its key | AppKit keeps those accessors as `-_key`/`-_setKey:`, which Apple's key-value coding searches and ours did not | Foundation |
| One profile, "New profile" | `-[NSBundle pathsForResourcesOfType:inDirectory:]` returned leaf names; `-[NSWorkspace typeOfFile:error:]` returned the extension, not the type the application declares | Foundation; AppKit 0029 |
| Preferences without a toolbar | a nib's toolbar lost its items when its delegate outlet was connected (a delegate that names no items); item visibility classes | AppKit 0029 |
| Pop-ups of Preferences empty or wrong | content arrays were handed to `-addItemsWithTitles:` as strings; the selection was not re-applied when the content came later | AppKit 0031 |
| `-[__NSCFBoolean longLongValue]` unrecognized | CoreFoundation's boolean class answered only `-boolValue` | Foundation |
| 20 s of processor time at launch | every string measurement is a pass of the whole text system (5 ms); Terminal measures 1384 strings for its profiles | AppKit 0032: sizes of unbounded one-style strings are remembered |
| Each bell an exception report | `-[NSDisplay beep]` raised "not implemented" | AppKit 0029: said once |
| Output 0.3 s late, menus 0.8 s, launch 3.4 s | AppKit, CoreText, Onyx2D, CoreGraphics and WindowServer were compiled without `-O`; a CTFont per drawn line was a fontconfig search and a FreeType face; glyph advances loaded each glyph's outline; application fonts scanned three times per launch; a fallback font searched through every installed face | `patched.nix` (`-O2`), CoreText 0034, AppKit 0035, Onyx2D 0037 |
| The last lines of a burst stayed off the screen until the cursor blinked | a flush that found its three frames busy waited 100 ms on the main thread and was dropped | WindowServer 0036: presented on the next release |
| Command-Q did nothing | Terminal sends SIGHUP to the child of the process it started and waits for that process; our `login` exec'd the shell | `system/login.c`: login stays the parent; AppKit 0038 (`+[NSCountedSet setWithCapacity:]`) |
| Bold drawn regular, italic light | `-convertFont:toHaveTrait:` took the first face with the trait; a variable font's named instance was drawn from its default instance | AppKit 0038, CoreText 0034 |
| True colour as bands | `_os_feature_enabled_impl` answers no to every feature flag (Terminal's DirectColor) | libSystem `feature-flags.patch` (Finch's reader), `FeatureFlags/Domain/Terminal.plist` |
| Rows not redrawn stayed grey; one-pixel seams at row edges | Onyx2D clips to a bounding box; `-getRectsBeingDrawn:` listed less than was cleared; dirty rects of whole points at 150% | AppKit 0040 |
| Quit and close alerts cut ("Termi...") | the legacy alert functions were a Cocotron panel with the message in its title bar; NSAlert's text did not wrap; words broke inside (a kerning pair ended a layout fragment and a space opening the next was no wrap point) | AppKit 0044, 0045, 0041, WindowServer 0043 |
| Profile list empty, labels cut | every float of a NIBArchive nib read as 0 (`-decodeFloatForKey:` returned a double); an image's cache window took the current context while a view drew | AppKit 0046, 0041 |
| A changed preference was lost | a click changes the cell, which no key-value observation of the control hears; the selection proxy set the value on an array | AppKit 0047 |
| Command-G typed "g" in the find bar | the field's text view inserted Command keys; the client was not in the bar's responder chain | AppKit 0048 |
| Menus 390 ms from click to popup (with the CJK fonts installed) | a fontconfig listing of every installed face per font name at each opening (the shortcut glyphs' fallback); eight shadow fills of the whole menu | AppKit 0049, 0046 |
| Terminal's icon was AppKit's generic one | the pin's Onyx2D reads no PNG entries of an `.icns` | Onyx2D 0042 (upstream tip's change) |

## Taken from Finch
Finch (an open Darwin for Apple Silicon; its own code "MIT OR Apache-2.0", SPDX per file) is cloned
at `sources/finch` (git-ignored). Taken from it, at Finch commit `2d9b1bb`, with its copyright and
SPDX notice kept in the file that carries it:

| Finch file | Where here | What |
|---|---|---|
| `userland/AppKit/NSWindowTabbing.m` | `cocoa/patches/0039-appkit-window-tabs.patch` (new file `AppKit/NSWindowTabbing.m`) | the structure of `NSWindowTab`, `NSWindowTabGroup` and NSWindow's tabbing calls: the group's bookkeeping (windows not retained, the window owns its group), the tabbing identifier's form, where `addTabbedWindow:ordered:` puts the tab, which tab is selected when one leaves, Move Tab to New Window, Merge All Windows. Adapted to this AppKit (manual retain/release, no `__weak`); the hand-over of one toplevel per group, the tab bar and the Window menu's tab items are ours |
| `userland/libsystem/featureflags/featureflags.c` | `libsystem-default/patches/feature-flags.patch` (new file `src/Libraries/libSystem/stub/pd_featureflags.c`) | the whole file unchanged: `_os_feature_enabled_impl` and `_os_feature_enabled_simple_impl` reading the FeatureFlags property lists (XML or binary, mapped, no allocation) |
| `userland/AppKit/NSTextFinder.m` | `cocoa/patches/0048-appkit-find-bar.patch` (new file `AppKit/NSTextFinder.m`) | the find bar view and NSTextFinder's actions; adapted to this AppKit's ivars (`NSMacOS26Additions.h`), manual retain/release, action constants, a container that is not a scroll view; Finch's NSTextView category not taken; the bar's own find actions are ours |
| `userland/AppKit/NSAlert.m` (`-layout`, `text_height`, `button_width`) | `cocoa/patches/0044-appkit-alerts-macos-layout.patch` (`AppKit/NSAlert.m`, notice above the function) | the alert's metrics and the order of its parts (420 points, 64-point icon, margins, button widths); written against this AppKit's ivars and string drawer |

## Performance, third round (14 November; on the M1, synthetic input, `tools/sf-latency.py`)
| Measure | Before this round (doc above) | Now | Note |
|---|---|---|---|
| Launch, exec to first commit | 0.97 to 1.06 s | 0.85 to 0.88 s (3 runs) | nib decoding not worked on |
| Output, 40 lines to commit / release | 66 to 72 ms / about 80 ms | 65 ms / 77 ms (86/104 the first time) | |
| Typing echo, key to commit | 15 ms | 7 to 8 ms | |
| Menu, click to its popup's commit | about 150 ms | 390 ms with the CJK fonts installed, about 195 ms after AppKit 0049 and the ring shadow | the old AppKit measured 387 ms under the same conditions: the fonts, not this round's AppKit, made it slower |
| New tab, Command-T to commit | - | 223 ms | |
| Tab switch, Command-Shift-[ | - | 47 to 102 ms | |
| Maximize, click to the first full-size frame | 218 / 137 ms | 219 ms | |
| Scroll with the wheel | a frame every 50 ms | not re-measured (the inject instrument has no wheel event) | |

## Sixth round (14 November, evening; COCOA3): the leak every program had, toolbar, scrollers, symbols
Seen on the M1 (synthetic input through the inject module while DESK2's test compositor ran; captures
`runs/sf-am3/shots/c3-t-*`, `c3-ts-*`, not committed):
- libFoundation's convenience constructors leaked every object (PureDarwin CoreFoundation's `CFAutorelease` never
  autoreleases a CF-owned object). Fixed in libFoundation (`PDAutoreleaseCF.h`), core round QEMU `c3b`/`c3b-2`
  (failed=0, QDDESK_PASS both), guarded install (libFoundation e77c9824, `.old6`; Foundation wrapper a660d750,
  `.old3`). Terminal ended at launch with it until cocoa 0108 (NSMenuItem's getters retained and autoreleased:
  Terminal keeps a removed menu item's key equivalent, which macOS keeps as a never-freed tagged pointer).
  Terminal with the new libraries: launch, 300,000 lines twice, Preferences, a new tab, the Inspector: alive
  (`c3-t-n2`, `n3`, `n4`). `sf_compat_push.sh check` after the installs: 18 of 18, `failed=0`.
- The Preferences toolbar shows its icons (Finch's symbol glyphs, cocoa 0111: General, Profiles, Encodings;
  Window Groups' symbol is none of them and stays blank) and the selected pane in a soft rounded rectangle (0113).
- Overlay scrollers (0114) for every application: no track, a thin knob shown when the user scrolls, faded a
  second later; the legacy look with `AppleShowScrollBars=Always` in the defaults or `PD_SCROLLERS=legacy`.
  Checked in Activity Monitor's table (`c3-sd-*`); in Terminal's scrollback the knob was not captured (the window
  sat partly off the screen and the inject compositor was then taken down at the end of DESK2's night).
- Finch's constraint solver in place of ours was measured with `PD_SOLVE_TIME=1` (cocoa 0112): Terminal's
  Preferences, 206 whole-tree solves, 1574 ms with ours; with Finch's as a throwaway solver, 104 solves had taken
  10075 ms after 9 s. Not taken; it would pay off only with an incremental driver (keep the solver between layouts,
  change constants), which is not written.
- Revert: SF-AA, "Installed on the M1's disk, sixth round".

## Fifth round (14 November, afternoon): Activity Monitor's changes merged, Terminal checked
Activity Monitor's cocoa patches are in the main series now (0100..0107, `cocoa/patched.nix`;
docs/roadmap/issues/SF-AA-activity-monitor.md). The merge attempt of the night had been taken back
because Terminal's Preferences General pane and Inspector Info pane overlapped with them. The cause:
with 0104's NIBArchive booleans read the right way round, those views are laid out by their
constraints, and three inputs to the solver were wrong. 0106 fixes them: a nib's standard-space
constraints (`NSSymbolicConstant` "NSSpace"; 246 of the 692 constraints of `TTAppPreferences.nib`)
were 0 points; archived hugging and compression priorities were not read; `NSMatrix` reported
10000 by 10000 as its intrinsic size. The Inspector's table header also lost its last column's title
under an empty corner.
- Seen on the M1 with the installed Terminal after the installs (fresh boot, synthetic input):
  Preferences General (`runs/sf-am3/shots/t3-prefs.png`, `t5-prefs.png`) and Inspector Info
  (`t4-info.png`, `t5-info.png`) are laid out as in `merge-before-*`, with the radio buttons and the
  checkbox in the accent colour and the Command field's placeholder. Tabs (`t4-tabs`), the View menu
  (`t4-view`) and full screen in and out (`t4-fs`, `t4-fsback`) also look right. The window comes back
  larger after full screen, as in the fourth round's `fsOff`.
- `sf_compat_push.sh check`: 18 of 18, `failed=0`, before and after the core installs
  (`runs/sf-am3/regress/`).
- Core files, rehearsed in QEMU `am1`/`am1-2` (failed=0, QDDESK_PASS both) and installed by the
  guarded installer: libSystem ba8013eb (`memorystatus_get_level`; `.old5` is the one of before),
  the CoreFoundation and Foundation wrappers with Activity Monitor's additions (6f280f66, d25ed0b1;
  `.old2`), and a new IOKit wrapper at `IOKit.framework/Versions/A/IOKit` (b5a239e7; the link of
  before is `IOKit.old`). IOKit's path is now one of the files the guarded installer handles
  (`sf_terminal.sh`, `sf_userland_push.sh`), with its probe. libFoundation bed36b5d: key-value
  observing remembers `+keyPathsForValuesAffectingValueForKey:` per class and key (rehearsed `am2`,
  `am2-2`; the one of before, 193b520a, is `.old5`; the probe's fifth check is the symbol
  `PDFoundationKVOAffectingKeysCache`). Found while writing that check, not fixed: a dependent key
  declared with `+keyPathsForValuesAffecting<Key>` is not heard when its affecting key changes through a
  plain setter, with this build and the one of before alike. `sf_compat_push.sh check` 18 of 18 after it.
- Replaced directly, with the file of before kept beside it: AppKit (`.pre-am` is TERM's 527784f2),
  CoreGraphics (`.pre-am`), the CoreServices wrapper (`.pre-am`). Details and how to revert are in
  SF-AA, "Installed on the M1's disk".
- Foundation-am replaces NSNumberFormatter's decimal and percent styles for every program that loads
  the Foundation wrapper, Terminal included; nothing in Terminal's checks changed with it.

## Fourth round (night of 14 November): performance, text size and keys, full screen
Everything here was done on the M1 booted from its disk. The input was synthetic (the compositor's
inject module); nobody looked at the panel. Pictures are in `runs/sf-terminal/shots/` (not committed).

What a person at the M1 can now do that they could not before:
- Scroll with the wheel at 60 frames a second (synthetic wheel events; the trackpad's scrolling was not tried). Menus open in 64 to 96 ms. Resizing the window by its border
  follows the pointer (about 25 ms from each compositor configure to the frame of that size).
- Change the text size: View > Bigger, Smaller and Default Font Size (Command-+, Command-=, Command--,
  Command-0). The window grows and shrinks with the text and keeps its 120x30 grid. Command-0 beeps
  when the size is already the default, as on macOS.
- Full screen: View > Enter Full Screen and Control-Command-F. The window covers the whole display
  in a Space of its own, with its menu strip. Exit Full Screen (the same key or menu item) brings it
  back (`fs1`..`fs3`, `w-Enter-Full-Screen-Ctrl-Cmd-F`).
- Use the panels: Shell > Export Text As (Command-S) and Open (Command-O). Print (Command-P) says
  that there are no printers and saves a PDF. New Remote Connection opens its window (all four in
  `t4-panels`).
- Use the Window menu: Minimize (Command-M) puts the window on the compositor's tile and a click on
  the tile brings it back (`w-Minimize-Cmd-M`, `min2`). Cycle Through Windows (Command-`) raises the next
  window. Tabs can be dragged to another place in the bar or out of the window into a window of
  their own (`tdrag0`..`tdrag2`).
- Use the Edit menu: Clear to Start (Command-K), Clear Screen, Select All, Find (`fd*`) and Use
  Selection for Find (Command-E) work.
- Open the inspector (Command-I): it lists the running processes by name ("login -pf root",
  "-zsh", "sleep 300", with their colours: `ins3`). Command-W closes it. Edit Title (Command-Shift-I)
  selects its title field (`t4-inspector-cmdw`: open, then closed by Command-W).
- See the other menu items: Terminal > Services shows "No Services Apply" (`svc1`). Secure
  Keyboard Entry toggles its check mark (`sk2`). New Command and New Remote Connection open their
  windows. Help says that no help is available instead of raising.
- Hear the bell: the sound goes through the sound stream's coreaudiod, as a short tone, because no
  macOS sound is copied. The audio server opened the speakers and mixed one client. Nobody listened.
- `open -a Terminal <folder>` opens the shell in that folder, not in its parent (installed
  libFoundation `193b520a…`, rehearsed in `ty11`).

### Performance (M1, `tools/sf-latency.py`, synthetic input)
"Before" is the stack installed at the start of the night (Ctrl-C, idle and memory: the libraries of
before in the test prefix `tpre`). "After" is the stack installed at the end. Each row is 2 to 6
measurements.

| Measure | Target | Before | After | Met? |
|---|---|---|---|---|
| Menu, press to the popup's first frame | < 100 ms | 122 to 164 ms | 64 to 96 ms | yes. The popup was drawn twice, and the window's menu strip was laid out again on every frame (at 150% the content's top edge falls into a pixel row of the strip); each key equivalent validated every menu |
| Launch, exec to the first frame | about 0.85 s, nib decoding and strip layout | 827 to 850 ms (2255 ms the first after a boot) | 818 to 933 ms | no. The profile: nib loading about 213 ms (key-value coding `setValue:forKey:` 63 ms, property-list parsing 33 ms, autorelease pool 45 ms), NSApplication's start 58 ms (a Wayland round trip 27 ms, the localization table 25 ms). |
| New tab (Command-T), to the new tab's first frame | near one frame | 169 to 171 ms | 137 to 175 ms | no. A new tab starts a shell on a pty and lays out a whole view. The rest of the time was not attributed |
| Tab switch (Command-Shift-[ / ]), first frame | near one frame | 58 to 62 ms | 44 to 54 ms (one 101 ms) | partly. It was drawn twice. Now 3 frames per switch (what each holds was not attributed) |
| Wheel, 60 steps one per compositor frame | 60 Hz | a frame every 31 to 33 ms, the last 400 ms behind | 60 frames for 60 steps, 14 ms median, 17 ms p90 | yes. The causes: the strip relayout; a CTLine and a CTFont made for each row of each frame (CoreText now keeps them); `-valueForKey:` formatting nine names per call (Foundation keeps the answer per class and key); rectangle outlines stroked through the edge rasterizer (Onyx2D covers axis-aligned rectangles and outlines directly); a 2 MB winding buffer calloc'd per context. In a tab with nothing to scroll: 31 frames for 60 steps |
| Resize by dragging the border, configure to the frame of that size | smooth | median 44 to 129 ms | median 24 to 26 ms, p90 30 to 39 (fresh window); 25 to 42 / 114 with the long scrollback `yes` and `find` leave | yes for a fresh window. Before, every configure was drawn in turn, at the old buffer's size limits |
| Maximize (frame button), click to the first full-size frame | - | 234 ms (restore 216) | 215 ms (restore 185); configure to frame 47 ms (26) | the client's part is 26 to 47 ms; the rest is before the configure arrives (compositor) |
| Typing echo, key to frame | keep 7 to 8 ms | 9 to 11 ms | 7 to 11 ms | kept |
| Output, 40 lines to the pty, to the frame / on screen | - | 63 to 73 / 74 to 84 ms | 49 to 69 / 62 to 83 ms | - |
| Control-C under `yes` / `find /`, to the process gone | at once | 86 / 115 ms (old libraries) | 61 / 39 to 43 ms (fresh window); 126 to 129 / 41 to 188 ms with the scrollback `yes` leaves | yes in a fresh window. A long scrollback slows everything that reflows or redraws |
| Selection drag, 30 pointer steps | - | not measured | 28 frames, 17 ms median gap | - |
| Flood, `cat` of 1.3 MB | - | 73 to 699 ms (noisy) | 297 to 880 ms (noisy) | not established either way |
| Idle CPU with a blinking cursor, 20 s | - | 0.21 s (old libraries) | 0.13 s (dev) to 0.32 s (installed, just after launch); RSS about 63 MB | - |
| Memory growth, 300,000 lines of output | - | +92 MB (old libraries) | +81 MB | - |

### Menus and key equivalents (Terminal's main menu, walked on the M1)
Each item was chosen by its key equivalent through the inject module, or by the pointer where it
has none. "Works" means the result was seen in a capture of the screen. "Commit only" means Terminal drew a new
frame and the content was not inspected. Not walked: no result either way.

| Menu | Works | Commit only | Unclear or wrong | Nothing | Not walked |
|---|---|---|---|---|---|
| Terminal | Preferences (1.2 s); Secure Keyboard Entry (the check mark; the private call it makes does nothing here, and on Wayland no other client receives the keys anyway); Services ("No Services Apply"); Quit | - | Hide Terminal (hides every window; nothing on the desktop brings a hidden application back); Hide Others (nothing to hide) | - | About Terminal; Show All |
| Shell | New Window; New Tab; New Command; New Remote Connection; Open; Close Window / Close Tab; Export Text As; Show Inspector; Edit Title; Print (no printers, Save as PDF) | Reset; Hard Reset | - | - | New Window / New Tab with Same Command; Use Settings as Default; Export Settings; Export Selected Text As; Edit Background Color (needs the colour panel); Print Selection |
| Edit | Copy; Paste; Select All; Clear to Start; Clear Screen; Find; Find Next; Find Previous; Hide Find Bar; Use Selection for Find; Copy Plain Text (beeps without a selection, as on macOS) | Clear Scrollback; Mark; Jump to Previous Mark; Jump to Next Mark | Paste Escaped Text, Insert Bookmark, Select Between Marks, Undo, Jump to Selection, Use Option as Meta Key (its HUD appears; the effect on keys not checked): no visible change | Show Colors (`NSColorPanel.nib` is missing) | Redo; Cut; Copy Without Background Color; Style for Copy; Paste Selection; Paste Escaped Selection; the Marks, Bookmarks and Navigate items other than those named; Clear to Previous Mark / Bookmark; Fill Screen; Find's Select All items; Num Lock |
| View | Show / Hide Tab Bar; Split Pane; Close Split Pane; Bigger; Smaller; Default Font Size; Enter / Exit Full Screen (menu and Control-Command-F) | Scroll to Top; Scroll to Bottom; Page Up; Page Down; Line Up; Line Down | Allow Mouse Reporting (no visible change) | - | Show Marks; Show / Hide Alternate Screen |
| Window | Minimize; Cycle Through Windows; Show Previous / Next Tab; Move Tab to New Window and Merge All Windows (third round); Zoom by the frame's button | - | - | Show All Tabs | Zoom (menu item); Open Window Group; Save Windows as Group; Return to Default Size; Bring All to Front |
| Help | Terminal Help (the alert that help is not available) | - | Open man Page for Selection (no result recorded) | - | Search man Page Index |
| Keys outside menus | typing; Return; Escape; Control-C; arrows | Page Up, Home, End | - | - | - |

Counts: 40 work, 12 drew a frame without being inspected, 10 unclear, 2 do nothing (Show Colors,
Show All Tabs). The rest were not walked: about 44 items, mostly alternates and the Marks,
Bookmarks and Navigate submenus.

Split Pane followed by Close Split Pane makes the window shorter each time, by 14 points per pair
(719, 710, 689 ... 599 pixels over three pairs). `sf-trace` shows Terminal's own geometry doing it:
`-[TTTabController splitPane:]` gives each pane 225 points (14 rows of 15 points), sets the window
to fit 2 x 14 rows, and Close sets it to fit the 29 rows that remain. Whether macOS, with its
line height, comes out the same was not checked. Show / Hide Tab Bar returns to the old height.

### Full screen
AppKit asks the compositor through `xdg_toplevel.set_fullscreen` / `unset_fullscreen` (AppKit
0052) and follows the compositor's state. The window's full-screen mask is AppKit's and is not sent to the
server as a style. The menu item reads Enter or Exit Full Screen, and the delegate gets the
will/did calls. The compositor's side was DESK's: client full-screen and maximize requests,
`xdg_activation_v1`, Command chords passed to Mac applications, and full-screen windows covering the
dock (`answers-from-DESK.md`, compositor patches 9 to 11). Open, the compositor's: after Exit Full Screen a second configure 120
to 145 ms later gives the window the work area's size (1629x1029 points) instead of its old frame
(seen again at the end with Control-Command-F).

### Left-overs
| Item | State |
|---|---|
| `open -a Terminal <folder>` in the parent | fixed. Terminal's `-openFolder:inNewTab:` keeps a URL that ends in "/" and otherwise takes the parent. Foundation's `+[NSURL fileURLWithPath:]` now ends a directory's URL in "/", as on macOS. The same check in `-initFileURLWithPath:` left the Preferences window empty, so that method is unchanged (the caller that depends on it was not found) |
| Use Selection for Find (Command-E) took one character | not a fault: the synthetic double-click's timing. With a proper double-click it takes the word (`findme123`) |
| Tab bar colour in dark profiles | as on macOS: Terminal sets no appearance on its windows (only on the Option-as-Meta HUD), so the bar follows the system appearance |
| Bell | audible through coreaudiod (AudioToolbox wrapper: System Sound Services, a short tone as the alert sound). Not heard by a person |
| Tab dragging | done: reorder within the bar, and drag out (more than 40 points away) to a new window |
| Services | the menu shows "No Services Apply"; there are no services |
| Secure Keyboard Entry | the item toggles; nothing to secure on Wayland (only the focused client receives keys; the test compositor's inject module is the exception) |
| Session restore | a relaunch restored a window with its working directory (it shows the `cd` it ran) |
| Marks | Mark and Jump to Previous / Next Mark draw; the marks themselves were not inspected |
| Profile thumbnails grey | not fixed. AppKit 0057 makes `-getRectsBeingDrawn:` answer the whole view outside a display; the thumbnails are still grey frames. Traced (`sf-trace`): `-[TTView thumbnailImageWithProfile:size:]` locks focus on a new image, concatenates its scale (0.16), and `-drawRect:` runs there over the whole 850x457 view. It then draws that image into a second image (`-drawAtPoint:fromRect:operation:fraction:` inside the second image's focus). Suspected, not established: an image whose only representation is a cached window copied into another image's focus gives nothing |
| Symbol images, `Assets.car` | not done. Finch's CoreUI reader is a whole framework, which is not adopted. Terminal's catalogue holds only its icon (read from the `.icns`) and colours; the toolbar's images are system SF Symbols |
| Font panel ("at least does not break") | it does not break, and it does not open: Preferences > Profiles > Text > Change… highlights and nothing follows (no `NSFontPanel.nib` installed; see the remaining-work table) |
| Scroller look, colour emoji (CBDT), window proxy icon, drag and drop, Show All Tabs, colour panel | not done |

### Faults found in the fourth round (in the system, none in Terminal)
| Symptom | Cause | Fix |
|---|---|---|
| Wheel at 30 Hz, menus 120 to 160 ms, tab switch 60 ms | the strip relaid out on every frame; popups and tabs drawn twice; a CTLine/CTFont per row per frame; `-valueForKey:` formatting names; rectangles through the edge rasterizer; a key equivalent validated every menu | AppKit 0051, 0052; CoreText 0053; Onyx2D 0054; Foundation (key-value coding cache) |
| A resize drag lagged hundreds of ms | every configure was drawn in turn; the size limits were committed on the old buffer, and the compositor clamped the next configure to them | AppKit 0052 (a configure overtaken by a later one is skipped); WindowServer 0058 (limits ride on the next frame) |
| Bigger / Smaller gave the wrong window size after the line cache | `-[NSFont isEqual:]` ignores the point size, so a line laid out at the old size was reused | CoreText 0053 compares the size (found in this round's own change) |
| Command-= did nothing | the item's key is "+" | AppKit 0052: Command-= is Command-+ |
| Every AppKit menu became a layer-shell overlay | upstream's layer-shell path, live once the compositor offered layer shell | WindowServer 0058: layer shell only with `PD_WS_LAYER_SHELL=1` |
| Export, Open, Print did nothing; Help raised | no panels without a display server's own; no print path; `NSHelpManager` unimplemented | AppKit 0055, 0056 |
| Command-` and the Window menu did not raise a window; Minimize lost the window | no `xdg_activation_v1` use; minimise was an unmap | WindowServer 0058, AppKit 0052 |
| Inspector raised exceptions; then listed `<TTProcessInfo: ...>` | `NSTableColumn -isHidden`, a cell copying a non-copyable value, font bindings, a non-colour foreground; value transformers named by class were never instantiated and a table column's binder ignored transformers | AppKit 0059 |
| Shift alone (and so Command-W) opened the inspector's pull-down | a button's empty key equivalent matched an event without characters | AppKit 0059 |
| `open -a Terminal <folder>` opened the parent | `+[NSURL fileURLWithPath:]` never ended a directory's URL in "/" | Foundation |

Patches: `cocoa/patches/0051`..`0059` (descriptions in `cocoa/patched.nix`),
`terminal-app/patches/foundation-macos-applications.patch`, `terminal-app/compat/AudioToolbox.c`.
Nothing was taken from Finch in this round.

### Remaining work (after the fourth round)
| Item | What is known | Estimate |
|---|---|---|
| Launch 0.8 to 0.9 s | nib decoding (key-value coding setters, property lists, autorelease) and NSApplication's start; the profile's figures are in the performance table | 1 day for a setter cache and the start's round trip |
| New tab 140 to 175 ms, tab switch 45 to 55 ms | three frames per switch; not attributed further | 1 day |
| Colour panel and Font panel (Show Colors, Edit Background Color, Preferences' colour wells and its font Change… button) | Show Colors does nothing and Change… only highlights. AppKit's own panels load from Cocotron's compiled nibs (`en.lproj/NSColorPanel.nib`, `NSFontPanel.nib`, `NSColorPicker.subproj/en.lproj/*.nib`, MIT, in the source), which upstream's recipe does not install. Laid into a test prefix's AppKit resources: Show Colors opened the Colors panel (its wheel drawn black), and Terminal then ended without a message when Preferences' Profiles pane opened (crash not diagnosed). Not installed | 1 day |
| Profile thumbnails grey | the drawing reaches the first image; the copy into the second is the suspect | 1 day |
| Show All Tabs | no tab overview in AppKit | 2 days |
| Exit Full Screen size | the compositor's second configure (work area) | compositor (`requests-for-DESK.md`, TERM 5) |
| Minimised-window tiles outlive their windows | two tiles stayed after their Terminal processes exited | compositor (TERM 5) |
| Hide Terminal | nothing on the desktop unhides an application | compositor / dock |
| Scroller look, colour emoji, proxy icon, drag and drop, symbol images | as before | as before |
| `-initFileURLWithPath:` of a directory | macOS ends it in "/"; doing so here broke the Preferences window | half a day to find the caller |
| Activity Monitor's patches into the shared stack (`requests-for-TERM.md`, "From ACTMON, 2") | tried. The cocoa patches 0100..0105 build over 0059, but with them Terminal's Preferences General pane and Inspector Info pane are laid out wrongly (`shots/merge-am-*` against `merge-before-*`). Likely cause: the NIBArchive booleans read the right way round hand those views to Auto Layout. Not merged. The wrappers, the IOKit wrapper and libSystem's `memorystatus_get_level` need the core pipeline: not done (`answers-from-TERM.md`, "To ACTMON, 2") | 1 day (constraint solver) + a core round |

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
| `/usr/bin/open` | `terminal-app/system/open.c` (`open --listen <pipe>`: the session's listener for the desktop menu's Applications rows) | new |
| `/usr/bin/login` | `terminal-app/system/login.c` (`login -pf <user>`, `-pfl <user> <program> ...`: only pre-authenticated logins; no utmpx, no "Last login") | new |
| the installer's probes (`/usr/share/sf-test/sf-*-probe`) | `terminal-app/probe/` | new |

`macos-compat/build-compat-root.nix` ("what a SiliconForest root gets for macOS programs") lays the
tree over its AppKit and Perl parts. The storage stream's tools lay only paths the base lacks
(`sf_userland_push.sh push sys`), so the files that replace the base's go on as a part of their own:
**`m1-storage/tools/sf_userland_push.sh push apps`** runs `sf_terminal.sh install new`, `install core`
and `install direct` in that order on the running root (and `verify apps` compares every file
there with the tree). The five files every program loads are only sent when their SHA-256 is in
`terminal-app/core-rehearsed.sha256`, the record of the QEMU rehearsals that passed
(`SF_USERLAND_DRY=1 ... push apps` checks that and prints what it would run). A fresh root also
gets root's shell (`m1-storage/userland/sf-userland-install.sh`: zsh where `/bin/zsh` is there),
the session's listener (`m1-storage/system/sf-session`) and, once it is installed as the desktop,
the compositor with popups and the Applications menu (`terminal-app/compositor/README.md`: built
from patches against the local repository, not by a recipe yet).

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
| 14 November. QEMU `ty6`, `ty6-2` | the second libFoundation (`ad2e75a9…`) with the unchanged other four: replaced as on the M1, `COMPAT_VERDICT failed=0`, `QDDESK_PASS`; fresh boot of the result: 38 verdicts ok, `failed=0`, `QDDESK_PASS` | `runs/m1-storage/qemu-dr/ty6/serial.log`, `ty6-2/serial.log`; `terminal-app/core-rehearsed.sha256` |
| M1: `sf_userland_push.sh push apps` | libFoundation replaced (`SFLIB 3 installed`, the one of before is `.old2`; its probe ends with status 139 on the first build and 0 on the second); the other four and dyld `already installed`; 80 direct files as they were | `runs/sf-terminal/m1-install/push-apps-2.txt` |
| M1: `sf_terminal.sh revert direct`, then `push apps` again | 56 entries reverted (47 new files and links removed, 5 replaced and 4 wrapped files renamed back: Terminal then stops at the first missing framework); installed again: 36 new files, 11 links, 5 replaced, 4 wrapped | `revert-direct.txt`, `push-apps-3.txt` |
| M1: `verify apps` | 80 of 81 files equal, one differs: `/usr/lib/libSystem.B.dylib` in 4 bytes (below) | `push-apps-3.txt` |
| M1: regression | `sf_compat_push.sh check`: 18 of 18 ok, `failed=0` | `runs/sf-terminal/regress/compat-check-1114.txt` |
| M1: the compositor | `/usr/bin/chonkstep-darwin-desktop` is the build with popups and Applications and without the inject module (`9115579a…`; the one of before, `721574af…`, is `.old-daily`); `.next` is the same source with the inject module (`810a1569…`) | `runs/m1-desktop-010/wm-darwin-apps-keep/` |
| 14 November, later. M1: regression after this round's installs | `sf_compat_push.sh check`: 18 of 18 ok, `failed=0` | `runs/sf-terminal/regress/compat-check-1114b.txt` |
| QEMU: the Cocoa stream's checks on patches 0001 to 0039 with `-O2` | `hello --scale 2 --theme graphite`, `frame --scale 2`, `popups --scale 2`, `apps --scale 1.5`: pass. `apps --scale 1.5 --theme amber-phosphor`: FAILED (the located operator-key box took in orange pixels at the Calculator's title, so the click landed below the keys; explained and fixed in the third round, below) | `runs/sf-terminal/regress/cocoa-*-1114b.txt` |
| M1: restart (test build, one marked boot) | from the desktop menu: Applications > Terminal; a command; Shell > New Window > Pro; Command-W twice ends the program | `shots/fin1`..`fin6` |
| M1: restart (installed build, no marks) | the session runs `/usr/bin/chonkstep-darwin-desktop` and the listener; no inject directory; a bundle path written into the session's pipe starts Terminal with zsh on a pty (the menu row itself was exercised in the test build of the same source). No capture of the screen exists for this boot: the installed build cannot make one | process list over SSH |
| 14 November, third round. QEMU `ty7`, `ty7-2` | libSystem `02f56b7e…` (feature flags) with the other four unchanged and the rehearsed dyld `472fd9a3…` (the recipe's rebuild of dyld differs only in its debug map: `sf_terminal.sh install-stage` with `SF_DYLD`): replaced as on the M1, `COMPAT_VERDICT failed=0`, `QDDESK_PASS`; fresh boot: 33 verdicts ok, `failed=0`, `QDDESK_PASS` | `runs/m1-storage/qemu-dr/ty7/serial.log`, `ty7-2/serial.log` |
| M1: `push apps`, then `install core` | the first push said libSystem "already installed" (its probe passed with the old library); the probe now also reads the flag: `install core` replaced libSystem (`SFLIB 3 installed`, the one of before is `.old4`; the ten programs at the statuses they had; tmux `-V` exits 1 with either library). `install new`: the fonts and the feature flag plist (the terminfo database was laid first by hand: `/private/var/db/sf-terminfo.installed`) | `runs/sf-terminal/m1-install/push-apps-y2.txt`, `core-y2.txt`, `new-y2.txt` |
| M1: AppKit, Onyx2D, libWindowServer, `/usr/bin/open`, the desktop terminal | replaced directly by `runs/sf-terminal/y2/put.sh` (the files of before are `<path>.pre-y2`); each the nix build of the patches committed (AppKit `e610800f…`, equal to the dev build) | - |
| M1: regression | `sf_compat_push.sh check`: 18 of 18 ok, `failed=0` | `runs/sf-terminal/regress/compat-check-y2.txt` |
| QEMU: the Cocoa stream's checks | `hello --scale 2 --theme graphite`, `frame --scale 2`, `popups --scale 2` (AppKit up to 0049), `apps --scale 1.5`, `apps --scale 1.5 --theme amber-phosphor` (up to 0050): all pass. The amber-phosphor failure of before was the check's: the compositor draws window titles in amber there, a few anti-aliased edge pixels of the Calculator's title fell within the tolerance of the operator keys' orange and the located box took in the title bar (1.5 px/pt measured as 3.83). The Cocoa stream recorded apps passing at 100% nextstep-classic, 150% teal-blueprint and 200% graphite, never at amber-phosphor. `cocoa_run.py`'s locate now leaves out rows and columns with only a few matching pixels: 1.52 px/pt | `runs/sf-terminal/regress/cocoa-*-y2.txt`, `cocoa-y2-summary.txt` |
| Night of 14 November, fourth round. QEMU `ty8` | libFoundation with the key-value coding cache: FAILED (`COMPAT_VERDICT failed=2`): its probe's 200 ms timing bound is too slow under TCG. The probe now finds the cache by the symbol `PDFoundationKeyValueCodingCache` | `runs/m1-storage/qemu-dr/ty8/serial.log` |
| QEMU `ty9`, `ty9-2` | libFoundation `5fdf0875…` with the other four unchanged: replaced as on the M1, `COMPAT_VERDICT failed=0`, `QDDESK_PASS`; fresh boot: `failed=0`, `QDDESK_PASS` | `qemu-dr/ty9/`, `ty9-2/`; `core-rehearsed.sha256` |
| M1: `install core libFoundation.dylib` | `SFLIB 3 installed`; the one of before is `.old3` | commit `1c191fd87` |
| M1: AppKit, CoreText, Onyx2D, libWindowServer, AudioToolbox wrapper | replaced directly by `runs/sf-terminal/t4/put.sh` (the files of before are `<path>.pre-t4`), each the nix build of the committed patches; AppKit and libWindowServer twice more as patches were added (last: AppKit `527784f2…`, equal to the dev build that was tested) | `runs/m1-coord/log.txt` |
| QEMU `ty10` | libFoundation that also asked the file system in `-initFileURLWithPath:`: first pass `failed=0`, `QDDESK_PASS`; stopped in the second pass, because on the M1 (test prefix) this build left Terminal's Preferences window empty. Not installed | `qemu-dr/ty10/` |
| QEMU `ty11`, `ty11-2` | libFoundation `193b520a…` (`+[NSURL fileURLWithPath:]` of a directory ends in "/"; the probe checks it) with the other four unchanged: replaced as on the M1, `COMPAT_VERDICT failed=0`, `QDDESK_PASS`; fresh boot: `failed=0`, `QDDESK_PASS` | `qemu-dr/ty11/`, `ty11-2/`; `core-rehearsed.sha256` |
| M1: `install core libFoundation.dylib` | `SFLIB 3 installed`, the one of before (ty9's) is `.old4`; the probe 1 with the library in use (it lacks the fourth build), 0 with the new one; node, claude, python3, bash, git, vim, nvim 0 with both, tmux `-V` 1 with both. Then `open -a Terminal <folder>` (as `-NSOpen`) gave the folder, and Preferences drew | `runs/sf-terminal/m1-install/core-t4.txt` |
| M1: regression | `sf_compat_push.sh check`: 18 of 18 ok, `failed=0` | `runs/sf-terminal/regress/compat-check-t4.txt` |
| QEMU: the Cocoa stream's checks (AppKit up to 0059) | `hello --scale 2 --theme graphite`, `frame --scale 2`, `popups --scale 2`, `apps --scale 1.5 --theme amber-phosphor`: pass. `apps --scale 1.5`: FAILED the first time (the typed command's `COCOATERM_OK` did not arrive within 120 s; the keys reached the window; two other QEMU guests ran at the same time), passed when run again alone | `runs/sf-terminal/regress/cocoa-*-t4.txt`, `cocoa-t4-summary.txt`, `cocoa-apps_scale_1.5-t4-rerun.txt` |

The dyld and libSystem that were rehearsed and installed are what
`libsystem-default/build-libsystem.nix` builds (sha256 `472fd9a3…` and `dfc854f1…`); libFoundation
`ad2e75a9…` (13 November: `ac8a048b…`), the wrappers `525419ad…` and `c4d12d0d…`. Not replaced:
libobjc, libCoreFoundation (unchanged) and `/usr/lib/system/libdyld.dylib`: the recipe's and the
installed one have the same code and data. Their 570 differing bytes are the modification times of
31 object files in the debugging symbol table (`N_OSO` entries: two builds of the same source, 12
and 14 November) and the code signature's hashes of the 15 pages that hold them. Nothing to install.

**The installed libSystem reads back with 4 bytes changed.** `/usr/lib/libSystem.B.dylib` on the M1
was verified as `dfc854f1…` before it was installed; read now it is `e5267447…`: four instruction
words in one page of `__text` (file offsets 0x27c000, 0x27c0a8, 0x27c210, 0x27c31c) are `nop`
(`d503201f`) where the file has `xpaclri` (`d50320ff`). That is what upstream's kernel does to
pointer-authentication instructions in pages it maps executable (section "arm64e"), here visible
through `read(2)`. Whether the page on the disk changed or only the cached page was not established
(it needs the raw block; the kernel is not this job's). Consequences: a hash of a library in use
is not a reliable check on this kernel, and `verify apps` names this file each time.

## On the M1's disk and how to revert
- The five core files: `ln <path>.old <path>.back && mv -f <path>.back <path>` each (libSystem's is
  `.old3`; libFoundation's `.old2` is the first patched build, `.old` the base's; for the two
  framework paths the kept file is a link: `ln -s "$(readlink <path>.old)"
  <path>.back && mv -f <path>.back <path>`). dyld first if all are to go: the old dyld starts every
  program with any of the libraries.
- Everything `install new` and `install direct` put there: `sf_terminal.sh revert direct`
  (`/private/var/db/sf-macos-apps.installed` is the list; new files are removed, `.old-sfy` and
  `.pd` files renamed back).
- `/Applications/Utilities/Terminal.app`: remove it. `/var/root/Library/Preferences/com.apple.Terminal.plist`
  and `/var/root/Library/Logs/Terminal.log` are what running it leaves.
- `/usr/bin/login`: remove it (it was put there before the installer's list existed and is not on
  it; Terminal then starts `/bin/sh` again).
- Root's shell: `/etc/passwd.old-sfy` and `/etc/master.passwd.old-sfy` are the files of before
  (`/bin/sh`).
- The session script: `/usr/libexec/sf-session.before-apps` is the one without the listener,
  `sf-session.old` the one of 13 November morning.
- The compositor: `ln /usr/bin/chonkstep-darwin-desktop.old-daily /usr/bin/chonkstep-darwin-desktop.back
  && mv -f /usr/bin/chonkstep-darwin-desktop.back /usr/bin/chonkstep-darwin-desktop` brings back the
  build of 12 November (no popups: then Mac applications have no menus, and no Applications row).
- The test compositor is `/usr/bin/chonkstep-darwin-desktop.next`. It runs only in the one boot that
  finds `/private/var/db/desktop-next-try`, and takes requests only when `/private/var/db/desktop-inject`
  exists. Both marks were removed on 14 November: the M1 runs the installed build, which has no
  inject module. To test again: `touch` both marks as root and restart (the first is renamed to
  `desktop-next-tried` when it is used).
- From the first day, from the Cocoa stream's root: `/System/Library/Fonts/{Inter,SourceCodePro,URW}`,
  `/etc/fonts/**`, `/share/fontconfig/**`, `/System/Library/Themes/Default.plist`,
  `/var/cache/fontconfig` (`runs/sf-terminal/m1-data-files.txt`, 129 paths). Fonts are open:
  Inter 4.1 (SIL OFL 1.1), URW base35 (AGPL-3.0 with the font exception), Source Code Pro (SIL OFL
  1.1). Nothing from macOS but Terminal.app itself; the font Terminal draws with is inside its bundle
  and is read from there (`PDApplicationFonts=NO` in the environment or its defaults turns that off).
- 14 November, later (this round): AppKit, CoreText, Onyx2D, CoreGraphics and libWindowServer of
  before are `<path>.pre-perf` (hard links; `ln -f <path>.pre-perf <path>.back && mv -f <path>.back
  <path>` each), the base's are still `.old-sfy`; `/usr/bin/login.before-fork` is the login that
  exec'd the shell; the compositors of before are `/usr/bin/chonkstep-darwin-desktop.before-cmdq`
  and `.next.before-cmdq`. `/var/cache/fontconfig` holds the cache of Terminal's font directory
  (written at its first launch). Test copies: `/private/var/db/sfy/{devO,wsdev*,pre}` (remove).
- `/private/var/db/sfy` (the scratch prefix, `/private/tmp/sfy` links to it after `ln -sfn`): remove.
- 14 November, third round: AppKit, Onyx2D, libWindowServer, `/usr/bin/open` and
  `/usr/bin/chonk-terminal-darwin` of before are `<path>.pre-y2` (`ln -f <path>.pre-y2 <path>.back &&
  mv -f <path>.back <path>` each); libSystem of before is `/usr/lib/libSystem.B.dylib.old4` (as for
  the core files above); the terminfo files laid by hand are listed in
  `/private/var/db/sf-terminfo.installed` (remove each); the fonts and the feature flag plist are
  on `/private/var/db/sf-macos-apps.installed` (`sf_terminal.sh revert direct`). Terminal's
  preferences file holds what the tests changed (window frames; CursorBlink was set and unset).
- Fourth round (night of 14 November): AppKit, CoreText, Onyx2D, libWindowServer and the AudioToolbox
  wrapper of before are `<path>.pre-t4` (hard links: `ln -f <path>.pre-t4 <path>.back && mv -f
  <path>.back <path>` each; AppKit `/System/Library/Frameworks/AppKit.framework/Versions/A/AppKit`,
  CoreText and Onyx2D likewise under their frameworks, `/usr/lib/libWindowServer.dylib`,
  `/System/Library/Frameworks/AudioToolbox.framework/Versions/A/AudioToolbox`). Installed now:
  AppKit `527784f2…`, CoreText `348df207…`, Onyx2D `872a34b6…`, libWindowServer `84eb8bf4…`,
  AudioToolbox `ac4f1f0a…`. libFoundation (as for the core files above): installed now `193b520a…`
  (ty11); ty9's (the key-value coding cache alone) is `/usr/lib/libFoundation.dylib.old4`; the
  build of before the cache is `.old3`. Test prefixes `/private/var/db/sfy/{tdev,tpre,tdevnf}`
  (remove). Terminal's preferences file holds what the tests changed (window frames, a restored window, the Secure
  Keyboard Entry default set and unset). `/private/tmp/oaf` is a test folder.

## Launching
From the desktop menu (right click on the desktop, or Command-Space): **Applications > Terminal**.
The submenu has one row per bundle in `/Applications` and `/Applications/Utilities`
(`compositor/wm-darwin-applications.patch`). The compositor starts no program: it writes the
bundle's path into the pipe `$XDG_RUNTIME_DIR/open-requests`, and the session's listener
(`open --listen`, started by `sf-session` with the desktop's environment) opens it; without a
listener the row says that the session takes no requests. Or type `open -a Terminal` /
`open /Applications/Utilities/Terminal.app` in a terminal or over SSH (`/usr/bin/open`: the bundle's
executable in a session of its own, output in `~/Library/Logs/<name>.log`). Each `open` starts a
new instance: there are no Apple Events to hand a request to a running one.

## Tools
- `tools/sf_terminal.sh`: stage, push, run, shot, scan (scratch prefix); install-stage, qemu,
  install, launch, revert.
- `tools/sf_inject.sh`, `tools/sf_keys.py`: typed text, keys, chords, pointer and captures of the
  screen through the compositor's inject module (`compositor/wm-darwin-inject.patch`, against the
  source of the compositor the disk has; built with `chonkstep-darwin/tools/build_desktop_binaries.sh`).
  `m1-storage/system/sf-session` runs `/usr/bin/chonkstep-darwin-desktop.next` for one boot when
  `/private/var/db/desktop-next-try` exists and gives the compositor its inject directory when
  `/private/var/db/desktop-inject` exists.
- The compositor with the inject module is a test build: `compositor/README.md` says how it is
  built, how it is kept out of daily use, and how to run it for one boot. Captures of the screen
  need it.
- `probe/sf-kvo-probe.m`: the installer's proof of a new libFoundation; extend it when the library
  gains something the installed build lacks, or the installer will say "already installed".
- `probe/sf-trace.c` (+ `sf-trace-thunks.S`): the methods of chosen classes as a program calls them,
  with arguments, results, objects named, and times (`DYLD_INSERT_LIBRARIES`, `SFTRACE=...`). How the
  causes above were found.
- `tools/sf_methods.py`: where an application's methods are and what each calls (reads the binary).
- `tools/sf-catch.c`: runs a program and reports a Mach exception from outside.
- `probe/sf-nibprobe.m`: decodes a compiled nib as NSNib does and names what stands in the way.
- `homebrew/sf_gapscan.py --app`.
- `probe/sf-prof.c` (`libsfprof.dylib`, built by `stage`): a sampling profiler of the main thread
  (`DYLD_INSERT_LIBRARIES`, `touch /tmp/sfprof.on` to start, `rm` to stop and write `/tmp/sfprof.txt`;
  `SFPROF_FOCUS`, `SFPROF_PROGRAM`). How every cause of slowness above was found.
- `tools/sf-latency.py` (on the target): `launch`, `output N`, `inject REQUEST`: ms from exec, from a
  write to the pty, from an injected request to the WindowServer's commits (`PD_WS_TIMING=1`).
  `trace REQUEST [settle]`: every frame of each surface after a request (count, sizes, gaps) and
  the time from each compositor configure to the frame of that size (drags, wheel, floods);
  `ctrlc PROGRAM`: Control-C while PROGRAM runs, ms until it is gone. `sf-prof` also writes each
  sample's whole stack (`/tmp/sfprof.txt.stacks`) for inclusive times.

## Commands
```sh
T=integrations/puredarwin/terminal-app/tools/sf_terminal.sh
$T stage && $T scan && $T push          # the tree, tried from /private/tmp/sfy (nothing installed)
$T install-stage                        # runs/sf-terminal/install
$T qemu NAME && $T qemu NAME second     # the rehearsal and the fresh boot of its result (25 + 20 minutes)
$T install new; $T install app; $T install core; $T install direct
integrations/puredarwin/m1-storage/tools/sf_userland_push.sh push apps   # the three installs, core files only as rehearsed; verify apps
$T revert direct                        # undo install new and install direct (then push apps again)
$T launch 20                            # or on the target: open -a Terminal
I=integrations/puredarwin/terminal-app/tools/sf_inject.sh
$I 'text:echo hello\n' key:up chord:ctrl+c wait:500 capture:name     # runs/sf-terminal/shots/name.png
```
Pending: the M1's own Terminal (macOS 27.0), which needs macOS running to fetch.
