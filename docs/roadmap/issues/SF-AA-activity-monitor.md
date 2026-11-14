# SF-AA: Apple's Activity Monitor on SiliconForest (GitHub issue 53)

## Goal
The unmodified Activity Monitor.app (macOS 26.5.2 copy from the controller Mac, version 10.14,
`private/mac-apps/m5-26.5.2/Activity Monitor.app`, git-ignored) runs on the SiliconForest desktop of
the 13" M1 MacBook Pro and shows SiliconForest's real processes with real CPU and memory figures.
The binary is never committed. Method and tools are Terminal.app's (SF-Y,
`docs/roadmap/issues/SF-Y-terminal-app.md`); everything here is in `integrations/puredarwin/activity-monitor/`.

## State
(kept current below; the newest round first)

- 14 November, sixth round (COCOA3; on the M1 booted from its disk; synthetic input through the inject module
  while DESK2's test compositor ran, nobody looked at the panel; captures in `runs/sf-am3/shots/c3-*`, crops in
  `runs/sf-cocoa3/`, not committed). **A leak every program had is gone**: PureDarwin's CoreFoundation's
  `CFAutorelease` returned its own objects without autoreleasing them, so every Foundation convenience
  constructor (`+array`, `+stringWith...`, `+dictionary`...) leaked. Activity Monitor grew 14 MB a minute
  (77.8 to 97.2 MB over 80 s); with the fixed libFoundation it stayed at 75.3 MB (same bundle, same 80 s,
  `runs/sf-cocoa3/amrss.sh`). Core round: QEMU `c3b`, `c3b-2` (failed=0, QDDESK_PASS both), guarded install.
  Then AppKit: the toolbar's stop/info/actions buttons as macOS 26 draws them (owner's photo: one capsule,
  borderless glyphs, one height and centre line, even spacing, a small down chevron; before/after:
  `runs/sf-cocoa3/toolbar-before-zoom.png`, `toolbar-after2-zoom.png`); overlay scrollers (no horizontal
  scroller where the columns fit, no black corner square, a thin knob that appears when scrolled and fades a
  second later: `c3-sd-rest/scroll/faded`); Window > CPU Usage and CPU History open (they ended the
  application or did not open) with Finch's NSCollectionView, but show no content yet (below); images of the
  application's `Assets.car` (`CPUUsagePill` is found). Measurements: table "Performance", second part.
- 14 November, fifth round (one agent; on the M1 booted from its disk; synthetic input through the
  compositor's inject module, nobody looked at the panel; captures in `runs/sf-am3/shots`, not
  committed). **The installed, unmodified bundle runs without any stage or `DYLD_*`**: on a fresh boot,
  `open -a "Activity Monitor"` (with only the session's environment: `env -i HOME XDG_RUNTIME_DIR
  WAYLAND_DISPLAY PATH`) shows its window with SiliconForest's processes (`b2-open`). What made it run:
  cocoa 0100..0105 in the main series, 0106 (below: the layout fault the nib booleans exposed in
  Terminal), the CoreFoundation/Foundation/CoreServices wrappers with Activity Monitor's additions, the
  new IOKit wrapper and libSystem's `memorystatus_get_level`, the core files through the QEMU rehearsal
  (`am1`, `am1-2`: `COMPAT_VERDICT failed=0`, `QDDESK_PASS` both) and the guarded installer. Terminal on
  the same boot: Preferences, Inspector, tabs, View menu, full screen as before (`t3-*`, `t4-*`, `t5-*`);
  `sf_compat_push.sh check` 18 of 18, `failed=0`, before and after. Then 0107 and the libraries:
  toolbar symbols (our drawings), the toolbar no longer cut, no "Hidden" menu title, rows without a
  stray icon, no black area right of the last column or below the last row, Energy tab rows (with View >
  All Processes; an estimate, see libsysmon), per-process network bytes, Send Signal, Sample's window
  opening (no sample), AM's own CPU (table "Performance"). Menus and tabs walked: table "Menus and tabs".
- 14 November, fourth round (M1, synthetic input only, captures in `runs/sf-actmon/shots`, nobody
  looked at the panel): clicking a column header sorts (AM sorts in its
  `-outlineView:shouldSelectTableColumn:`, which this AppKit never asked; `sort1/sort2`); typing in
  the search field filters the table (`search2`: only `sleep`, PID 380); a click selects a row
  (`sel2`); Command-I opens the Process Inspector (`inspect1`: "sleep (380)", parent launchd (1),
  user root (0), process group, Real Memory 3.3 MB, Virtual 391.08 GB); Option-Command-Q on a test
  `sleep` started over SSH shows "Are you sure you want to quit this process?" with Cancel / Force
  Quit / Quit (`quit1`), Return quits it (the process was gone 10 s later, `pgrep`); Force Quit on a
  second test process (`/private/tmp/sfam-victim`, a copy of sleep) ended it too (`fq1`); the View
  menu and its Update Frequency submenu open (`m1`, `m2`, `steps`), "Very often (1 sec)" takes
  effect; the title bar's maximise button resizes the window and AM lays out again (`max1`); wheel
  scrolling moves the table (`scroll1`); the Energy tab opens (`energy2`) but lists no rows. AM's
  own CPU at the default 5 s interval: 41 % of a core before this round's changes, 27 % after
  (`ps` time over 30 s); at 1 s, 86 %. Open: the Energy tab's rows; per-process network bytes 0;
  right of the last column stays black after maximising; the Actions pull-down's menu shows open at
  launch, and after the first clicks a "quit these processes?" confirmation (the Stop button's
  `quitProcess:`, nothing selected) appears in some runs (cause not found; Cancel dismisses it);
  toolbar symbol images blank (SF Symbols).
- 14 November, third round (M1, synthetic input, captures `runs/sf-actmon/shots/am22.png`): the
  CPU tab shows real %CPU (Activity Monitor itself 43.6, the compositor 13.3, kernel_task 5.4 ...),
  sorted by %CPU as macOS's default, CPU time as m:ss.hh, idle wake-ups, names cut with an
  ellipsis; the bottom pane has the real System/User/Idle percentages (4.63 % / 14.22 % / 81.15 %),
  the CPU load graph, Threads 181 and Processes 33. Faults behind it (table below): NIBArchive
  booleans read inverted, a cell's line break mode cut to two bits, custom views' constraints not
  decoded, value transformers named by class not made, the number formatter's styles, libsysmon's
  row applier stopping early, a whole-window constraint solve per table cell (AM at 100-200 % CPU).
  Open: the table header draws black, AM's own CPU cost, the toolbar's symbol images, an action
  menu that sometimes shows at launch.
- 14 November, second round (on the M1, synthetic input only, nobody looked at the panel): dyld
  reaches `main` from the stage (`sf-prof` samples: `NSApplicationMain` -> `-[NSApplication init]`);
  the main window is on the compositor with its toolbar (CPU, Memory, Energy, Disk, Network), the
  CPU tab's columns, and **rows for SiliconForest's real processes**: name, CPU time, threads, Kind
  ("Apple"), PID, user (`runs/sf-actmon/shots/am8.png`: kernel_task 120 threads 55.93 s, launchd,
  sshd, Terminal, pd-netd, coreaudiod, ...). Open at this point: %CPU shows 0.0 everywhere, three
  rows have no name, the bottom summary pane is not visible, toolbar symbols are blank (SF Symbols).
  Faults found on the way (all in the system): below, "Faults found".
- 14 November, first round: gap scan, all 101 missing imports provided in a stage
  (`runs/sf-actmon/stage`, scratch prefix `/private/var/db/sfam` on the target). Scanner against the
  stage: **0 of 405 imports missing, 0 classes missing** (`runs/sf-actmon/scan/actmon-stage1.md`).
  Nothing has run on the M1 yet at this point of the document.

## Installed on the M1's disk, sixth round (14 November, evening; `/private/var/db/sfcocoa3/installed.txt`)
- Core files by the guarded installer after QEMU `c3b`/`c3b-2`: `/usr/lib/libFoundation.dylib` e77c9824 (the one of
  before, bed36b5d, is `.old6`), the Foundation wrapper a660d750 (without its own NSIndexPath; `.old3`).
  `sf_compat_push.sh check` after: 18 of 18, `failed=0` (`runs/sf-cocoa3/check-after-core.txt`).
- Replaced directly: AppKit cfd8699a (0108..0115; `.pre-c3` = 17180649 the fifth round's, `.pre-c3b` = 1978825a
  with 0108 only, `.pre-c3c` = bdc54d59 with 0108..0114), CoreGraphics 87682a2e (0109; `.pre-c3` = 235b866c). AppKit needs this libFoundation (NSIndexPath).
- New path: `/usr/lib/libcompression.dylib` a70c5411 (Codex's whole Finch libcompression; licences in
  `/usr/share/licenses/finch-libcompression`).
- Revert: AppKit/CoreGraphics `ln -f <path>.pre-c3 <path>.back && mv -f <path>.back <path>` (AppKit first: an
  AppKit of this round with the libFoundation of before does not load); core files as SF-Y; libcompression `rm`.

## Installed on the M1's disk (fifth round, 14 November; replaces the section of 13:14Z)
- The unmodified bundle at `/System/Applications/Utilities/Activity Monitor.app` (a link in
  `/Applications/Utilities`), as installed at 13:14Z: 35 files with the private copy's hashes. The dock's
  `com.apple.ActivityMonitor` tile shows now that the bundle is there (its click did not start the
  application on this boot: the dock never ran its `Exec`, see "Still wrong").
- The new libraries of 13:14Z at paths the disk lacked (`/private/var/db/sfam/installed.txt`), three of
  them replaced since, each file of before kept beside it as `<path>.pre-am`: `/usr/lib/libsysmon.dylib`
  (energy estimate), `/usr/lib/libquit.dylib` (the helper in process),
  `/System/Library/PrivateFrameworks/NetworkStatistics.framework/Versions/A/NetworkStatistics`.
- Replaced directly (not loaded by every program), the file of before kept beside it:
  `/System/Library/Frameworks/AppKit.framework/Versions/A/AppKit` (`.pre-am` = 527784f2, TERM's of the
  fourth round; `.pre-am2` = 223acb2e, 0100..0106; `.pre-am3` = 4f88ae87; `.pre-am4` = c16162e4;
  installed 17180649 = `cocoa/patched.nix` with 0100..0107), `CoreGraphics.framework/Versions/A/CoreGraphics`
  (`.pre-am` = 6adb5f7f; installed 235b866c = with 0102), `CoreServices.framework/Versions/A/CoreServices`
  (the wrapper; `.pre-am` = fbd50608; installed ece77c8b; the real library stays `CoreServices.pd`).
- Core files, by the guarded installer after the rehearsals (`terminal-app/core-rehearsed.sha256`):
  `/usr/lib/libSystem.B.dylib` ba8013eb (the one of before is `.old5`), the CoreFoundation wrapper
  6f280f66 (`.old2`), the Foundation wrapper d25ed0b1 (`.old2`), `IOKit.framework/Versions/A/IOKit` b5a239e7
  (the link of before to `/usr/lib/libIOKitCF.dylib` is kept as `IOKit.old`), libFoundation bed36b5d (key-value
  observing remembers the affecting keys; rehearsed `am2`, `am2-2`: `failed=0`, `QDDESK_PASS` both; the one of before,
  193b520a, is `.old5`). `sf_compat_push.sh check` 18 of 18, `failed=0`, after each install.
  libSystem reads back as 64b36b12 once it has run: the kernel's lowering of pointer-authentication
  instructions (SF-Y, "arm64e"), verified before anything loaded it.
- Test copies (remove): `/private/var/db/sfam3` (test prefixes, logs, probes), `/private/tmp/sfam-victim`.
  Activity Monitor's preferences of before this round: `/var/root/Library/Preferences/com.apple.ActivityMonitor.plist.before-am3`
  (its column widths had been grown by the old `-tile`).

### Revert
- AppKit/CoreGraphics/CoreServices wrapper and the three libraries: `ln -f <path>.pre-am <path>.back && mv -f
  <path>.back <path>` (AppKit: `.pre-am` is the build of before this round).
- Core files: as SF-Y "On the M1's disk and how to revert" (`ln <path>.oldN <path>.back && mv -f <path>.back
  <path>`; for IOKit the kept file is a link: `ln -s "$(readlink <path>.old)" <path>.back && mv -f <path>.back <path>`).
  The CoreFoundation and Foundation wrappers of before (`.old2`) carry Terminal's additions only.
- The bundle and the libraries of 13:14Z: `tools/sf_actmon.sh uninstall`.

### The install of 13:14Z (for the record)
- The unmodified bundle at `/System/Applications/Utilities/Activity Monitor.app` (macOS's place), a link
  to it at `/Applications/Utilities/Activity Monitor.app`. Its 35 files read back with the private copy's
  SHA-256 each (`runs/sf-actmon/bundle-private.sha256`, `bundle-installed.sha256`), read before anything
  executed them. The bundle is streamed from the private copy: the first install copied it on the M1 from
  a copy that had run, and this kernel reads such a program back with its pointer-authentication
  instructions lowered to `nop` (1236 bytes differed); that copy was replaced and removed.
- The libraries at paths the disk did not have: `/usr/lib/libsysmon.dylib`, `libIOReport.dylib`,
  `libsystemstats.dylib`, `libquit.dylib`, `libLaunchServicesSupport.dylib`; frameworks Metal, OpenCL,
  SecurityFoundation, UniformTypeIdentifiers; private frameworks AssetCacheServicesExtensions,
  CoreSymbolication, CrashReporterSupport, NetworkStatistics, RemoteServiceDiscovery, SafeEjectGPU,
  Suggestions (list and hashes: `/private/var/db/sfam/installed.txt`). No file of the disk was replaced.
- Not installed (other streams' or core files; merge steps in `runs/m1-coord/requests-for-TERM.md`,
  "From ACTMON, 2"): AppKit (0100-0105), CoreGraphics (0102), the CoreFoundation/Foundation/CoreServices
  wrapper additions, the IOKit wrapper, libSystem's `memorystatus_get_level`. Until they are merged,
  `open -a "Activity Monitor"` finds the bundle (rc 0) but the program ends in dyld
  (`_IOPMCopyAssertionActivityAggregate` not in IOKit); it runs only from the stage
  (`/private/var/db/sfam`, DYLD_*).
- Revert: `tools/sf_actmon.sh uninstall` (removes every listed path, the bundle and the link).

## Faults found (all in the system, none in Activity Monitor)
| Symptom | Cause | Fix |
|---|---|---|
| Waits for ever in `-[NSApplication init]` | the SSH session's `XDG_RUNTIME_DIR` won over the desktop's | `guest/run.sh` sets the session's |
| `-[NSByteCountFormatter setFormattingContext:]` raised | formatters had no formatting context | `libs/Foundation-am.m` (NSFormatter) |
| SIGSEGV at a NULL function in OpenGL from `-[BatteryGraphView awakeFromNib]` (`setWantsLayer:`) | a layer context renders through CGL, which calls into EGL; the disk's root has no libEGL | cocoa 0103: without EGL a layer-backed view draws as an ordinary view |
| No rows: the outline view's data source stayed nil | `-[NSOutlineView setDataSource:]` raised for a source without `outlineView:objectValueForTableColumn:byItem:` (a view-based source does not answer it); the nib's outlet connection swallowed it | cocoa 0101 |
| No cells drawn | this AppKit had no view-based tables | cocoa 0101 |
| The first process table came after one update interval (5 s) | a running request's new interval also moved its next table | `libs/sysmon.m`: a table at once |
| Values after the first unknown attribute were missing | this libxpc keeps no null object in an array | `libs/sysmon.m`: the row keeps its values itself |
| Thread count 15360 | `kern.num_threads` is the limit | sum of the processes' threads |
| AM at 100-200 % CPU, the main thread in the constraint solver | every cell view's constraints made the whole window be solved, at each display | cocoa 0101: a table row view is the top of its own layouts and left out of the window's |
| %CPU 0.0, the bottom pane 102 points below the window | every BOOL of a NIBArchive nib read inverted (type 4 is false, 5 true: separator menu items carry `NSIsSeparator` as 5) | cocoa 0104 (`NSNibArchiveUnarchiver.m`; told TERM, `requests-for-TERM.md`) |
| Long process names blank | a cell's line break mode read from two of its three bits: truncating tail became word wrapping | cocoa 0104 (`NSCell.m`) |
| The summary box black, its figures one digit wide | a custom view (NSCustomView) lost its archived constraints; the box's colour is a named colour of the app's catalog | cocoa 0104 (`NSCustomView.m`); 0105 (Finch's catalog reader) |
| Percentages printed as 0.0583911453518234, threads 0 | bindings did not make value transformers named by class; the number formatter printed every style as `%.3f`; libsysmon's row applier stopped at the first attribute's answer | cocoa 0104 (`NSBinder.m`); `libs/Foundation-am.m`; `libs/sysmon.m` |
| A figure stayed as wide as its first value | a new value did not invalidate a constrained label's intrinsic size | cocoa 0104 (`NSControl.m`) |
| CoreFoundation halt opening the Energy tab | the responsible pid was left out of the row; AM's `objectForKey:` got nil | `libs/sysmon.m`: the process's own pid |
| Clicking a header did not sort; clicking a row did not select it | Cocotron's header never asked `-tableView:shouldSelectTableColumn:`, the rows did (AM sorts there and answers NO) | cocoa 0101 (`NSTableHeaderView.m`, `NSTableView.m`, `NSTableColumn.m`) |
| Typing in the search field did not filter | the search field sent no action while typing | cocoa 0100 (`-[NSSearchField textDidChange:]`) |
| Command-I raised `-[NSValue initWithCoder:]` | libFoundation's NSValue has no keyed decoding (the inspector's nib carries sizes) | `libs/Foundation-am.m` |
| An abort at start in some runs: `-[NSArray sortedArrayWithOptions:usingComparator:]` | missing from libFoundation's NSArray; AM's GPU handler calls it on a dispatch queue | `libs/Foundation-am.m` (a stable merge sort) |
| CoreFoundation halts in `CFArrayCreateCopy` under `-[NSNotificationCenter postNotification:]` at start (5 of about 15 launches) | bindings applied AM's model changes on its sampling queue: the text field binder took its notification observers off and on there while the main thread posted (found with `SFAM_NC_DEBUG=1`) | cocoa 0104 (`NSKVOBinder.m`, `NSCachingBinder.m`, `NSMultipleValueBinder.m`, `NSBinder.m`: a change made off the main thread is applied on it); 8 launches after it without a halt |
| AM at 41 % CPU at 5 s | a layout pass of the whole window for every changed figure | cocoa 0104 (`NSControl.m`: only when the shown string's length changes, and on the main thread): 27 % |
| (fifth round) Terminal's Preferences General and Inspector Info panes overlapped once 0104 read nib booleans right (their views now laid out by constraints) | a nib's standard-space constraints (`NSSymbolicConstant` "NSSpace", 246 of 692 in Terminal's preferences) were 0 points: Foundation's decoder does not read the key; the hugging and compression priorities nibs archive ("{250, 750}") were not read; `NSMatrix` answered NSControl's cell size, 10000 by 10000, as its intrinsic size (radio buttons squeezed the labels beside them) | cocoa 0106 (`NSLayoutConstraint.m`, `NSView.m`, `NSViewLayout.m`); found with `PD_LAYOUT_TRACE=1` (the solver's constraints and frames), compared with the nib's archived frames |
| Terminal's Inspector: the last column's title under an empty corner | the header clip view was always narrower by a scroller's width, the corner view drawn over it, also where no vertical scroller takes room | cocoa 0106 (`NSScrollView.m`) |
| `open -a "Activity Monitor"` ended in dyld (`_IOPMCopyAssertionActivityAggregate`) | IOKit's framework path was a link to libIOKitCF | the IOKit wrapper (`terminal-app/compat/build.sh` with `libs/IOKit-am.c`), a core file: rehearsed (`am1`), guarded install |
| Toolbar buttons cut at the top, symbols missing | an icon-only toolbar's item views were placed 4 points above their item (the label's padding without a label); SF Symbols are not here | cocoa 0107 (`NSToolbarItemView.m`, `NSToolbarView.m`; `NSSymbolImages.m`: our drawings of info.circle, ellipsis.circle, xmark.octagon, magnifyingglass, arrow.forward.circle.fill) |
| A "Hidden" menu in the menu strip | the strip's grouped bar left out hidden items only in the other path | cocoa 0107 (`NSMenuView.m`) |
| Every process row showed Activity Monitor's icon | `-[NSImageCell setImage:nil]` left the nib's archived object value (NSApplicationIcon) to draw | cocoa 0107 (`NSImageCell.m`): the image is the object value |
| Columns grew (Idle Wake Ups 267 points) and were saved so | `-[NSTableView tile]` fitted the last shown column whatever the autoresizing style; Activity Monitor's is none, and it hides columns per tab | cocoa 0107 (`NSTableView.m`) |
| Black right of the last column (maximised, other tabs) and black bands below few rows (search) | stripes were drawn only as wide as the columns, and below the last row from the dirty rect's top for its height | cocoa 0107 (`NSTableView.m`, `NSTableHeaderView.m`: an empty header there; row views span the table) |
| AM at 25 % CPU (5 s) | each reload removed and rebuilt every row view (all drawn again); a system colour looked up in the display's table at every fill; a getenv per `-setFrame:`; key-value observing asked `+keyPathsForValuesAffectingValueForKey:` (a selector built and looked up, the class chain walked) for every observation at every change | cocoa 0107: rows reloaded in place, the cell's own view given back (`NSTableViewViewBased.m`); colours resolved once per appearance (`NSColor_catalog.m`); libFoundation: the affecting keys remembered (`terminal-app/patches/foundation-macos-applications.patch`, core: rehearsed `am2`) |
| Send Signal sent nothing | the sheet's pop-up had no selected item (the nib archived none, items were put in its menu directly), Activity Monitor read the tag of no item: `kill(pid, 0)` | cocoa 0107 (`NSPopUpButtonCell.m`: a pop-up with items has one selected) |
| Per-process network bytes 0 | NetworkStatistics ran a query's completion at once, before the kernel's updates were read | `libs/NetworkStatistics.c`: the completion runs on the kernel's success message (or after a second) |
| (sixth round) AM grew 14 MB a minute; Terminal and every Foundation program leaked | PureDarwin CF's `CFAutorelease` returns a CF-owned object without autoreleasing it (the Linux runtime's hook, `argon_cf`, is not there on Darwin); libFoundation called it in every convenience constructor | libFoundation: `PDAutoreleaseCF.h` before every source (`CFAutorelease` sends `-autorelease`); the root cause in CoreFoundation is not changed (AppKit's NSEvent, CoreGraphics' CGEvent, CoreText's font manager and programs still call it) |
| (sixth round) Terminal ended at launch with the leak fixed | Terminal keeps a menu item's key equivalent after removing the item (on macOS a one-letter string is a tagged pointer, never freed) | cocoa 0108: NSMenuItem's object getters retained and autoreleased, as macOS's atomic properties |
| (sixth round) Window > CPU Usage ended the application; CPU History did not open | no modern NSCollectionView; nothing registered for the item identifier (macOS falls back to a nib or class of that name) | cocoa 0110 |
| (sixth round) toolbar buttons of different heights, bevelled, uneven, an up triangle on the pull-down | Cocotron's bezels; item views kept the nib's sizes; the leftover width went to the pull-down | cocoa 0113 |
| (sixth round) horizontal scroller and a black square where the columns fit | legacy scrollers always take room; the corner between them was never painted | cocoa 0114 |
| Energy tab without rows (its default filter, Applications in last 12 hours) | that list comes from systemstats' 12-hour history, which SiliconForest does not keep | not fixed: View > All Processes lists every process with the estimate (libsysmon) |

## Menus and tabs (fifth round, walked on the M1 with the installed bundle; synthetic input)
"works" = observed on the M1 in this round or the fourth (captures in `runs/sf-am3/shots` or
`runs/sf-actmon/shots`); "not inspected" = not tried; "broken" = tried and wrong.

| Menu / place | Item | State |
|---|---|---|
| Activity Monitor | the menu (About, Services, Hide, Hide Others, Show All, Quit) | works: opens with macOS's items (`m-app`) |
| | About Activity Monitor, Services, Hide, Hide Others, Show All, Quit | not inspected |
| File | Close (Command-W) | works, but differs from macOS: the window closes and the application then ends (SiliconForest ends an application whose last window closes, `PDQuitsWithLastWindow`); macOS keeps it running |
| | Page Setup, Print | not inspected |
| Edit | the menu (Undo, Redo, Cut, Copy, Paste, Delete, Select All, Find) | opens (`m-edit`); items not inspected |
| View | Columns | the submenu opens with the column list and check marks (`m-columns`); choosing a column not inspected |
| | Dock Icon | the submenu opens (`m-dockicon`); not applicable for now: the Chonkstep dock draws its own tile and shows no application's dock-tile content |
| | Update Frequency | works (fourth round: 1 s takes effect) |
| | All Processes | works (Energy tab lists every process) |
| | Applications in last 12 hours (the Energy tab's default) | broken: empty, there is no 12-hour history (systemstats) |
| | the other filters (Hierarchically, My, System, Other Users', Active, Inactive, GPU, Windowed) | not inspected |
| | Filter Processes (the search field) | works (typing filters: `s1`, `c17`) |
| | Inspect Process (Command-I) | works: the inspector of a selected process (`s3-insp`); its Executable Path is empty and its Memory pane's rows overlap (still wrong) |
| | Sample Process (Option-Command-S) | fails gracefully: the sample window opens and stays empty (`s15-sample`); there is no sample tool |
| | Run Spindump, Run System Diagnostics | not inspected (the in-process helper replies with an error) |
| | Quit Process (Option-Command-Q) | works: "Are you sure you want to quit this process?", Quit ended a test process (`s4-quit`) |
| | Send Signal to Process | works: the sheet (shown as a panel of its own, not attached), Hangup sent, the test process ended (`s13`, `s14`) |
| | Show Deltas, Clear CPU History | not inspected |
| Window | the menu | opens (`m-window2`) |
| | Activity Monitor (Command-1), CPU Usage (Command-2) ... (Command-5) | the keys belong to the desktop (Spaces 1 to 5); from the menu: |
| | CPU Usage | opens (sixth round, `c3-cu3b`); empty: its items draw through layers (Still wrong) |
| | CPU History | opens (sixth round); empty: compositional layout not here (Still wrong) |
| | GPU History, Afterburner History, Keep CPU Windows on Top, Arrange in Front, Minimize, Zoom | not inspected |
| Help | Activity Monitor Help | works: "Help isn't available for Activity Monitor." (`w-help2`) |
| Tabs | CPU, Memory, Disk | works (third and fourth rounds; CPU again this round) |
| | Energy | works with All Processes (estimate, see libsysmon); empty with its default filter |
| | Network | works: per-process bytes and packets (an SSH session: 4 KB sent, 5 KB received, `c17`); the system's Packets in/out and Data received are 0 (below) |
| Toolbar | symbols, search, tabs | works (drawn: `d1`); the Stop, Info and Actions buttons were not clicked this round |
| Table | header click sorts, row click selects | works (fourth round); double-click on a row not inspected |
| Launch | `open -a "Activity Monitor"` | works (fresh boot, no stage, `b2-open`) |
| | the dock tile | broken: a (synthetic) click did not start it; the dock ran no `Exec` (its log shows nothing) |
| | desktop menu > Applications | not inspected |
| (fourth round's faults) | the Actions menu open at launch, a "quit these processes?" alert | did not recur in this round's launches (about 15) |

## Performance (Activity Monitor's own CPU, `ps` time over 40 s, default 5 s interval, M1)
| Build | CPU tab | Other |
|---|---|---|
| fourth round (ACTMON's stage) | 27 % | - |
| this round, before 0107 (installed bundle, test prefix) | 25 % | - |
| 0107 in a test prefix, with the key-value observing cache (and a shortcut for views out of sight, taken out again: it left a black band) | 10 % | - |
| installed 0107, libFoundation of before | 17.5 to 19.7 % | Energy tab 7.5 %, Network 14.6 % |
| installed 0107 and libFoundation with the cache (final) | 10.3 % (42 processes) | - |
Sixth round, 10 minutes on the M1 (installed bundles started with `open -a`, AM at its default 5 s interval with
about 30 processes, Terminal one idle window with a blinking cursor; `ps` every 30 s,
`/private/var/db/sfcocoa3/measure-idle.txt`; no input during the run, nobody at the panel):

| | AM RSS | AM CPU | Terminal RSS | Terminal CPU |
|---|---|---|---|---|
| fifth round's libFoundation (leak), 80 s | 77.8 -> 97.2 MB (+14.6 MB/min) | - | - | - |
| this round, 540 s | 81.5 -> 88.8 MB (+0.8 MB/min) | 13.0 % | 61.5 -> 64.5 MB (+0.33 MB/min) | 1.1 % |
| with a pool drained each time the main run loop waits (tried, not installed), 330 s | 81.8 -> 85.2 MB (+0.6 MB/min) | 14.7 % | 58.0 -> 59.7 MB (+0.3 MB/min) | 0.9 % |

Still growing slowly, cause not found (a pool per run-loop wait, as macOS's AppKit keeps, changed little and was
taken out again: not walked through menus and modal panels without the inject compositor). AM's CPU went from
10.3 % (fifth round) to 13 %: the objects the leak kept are now freed (dealloc work), not attributed with sf-prof.
Terminal under continuous output was not measured. `-[NSEvent CGEvent]` leaked its event for the same reason
as libFoundation (cocoa 0115, installed).

Found with `probe/sf-prof.c` (main thread samples) and `sf-trace` (calls and times of chosen methods):
row views rebuilt and drawn at every reload, colour lookups, key-value observing's affecting keys,
a getenv per frame change. What is left: drawing text through the text system for each changed
label, the layout solve after a label's length changes, Activity Monitor's own formatting.

## Still wrong (in the order a person meets it)
- The dock tile does not start Activity Monitor (`open -a` and the session's listener do).
- Window > CPU Usage and CPU History open (sixth round) but stay empty: CPU Usage's 8 items are laid out by the
  flow layout (`PD_CV_TRACE=1`: 8 attributes, 296x106) and draw nothing (Activity Monitor draws them through
  layers; without EGL this AppKit draws a layer-backed view as an ordinary view, and layer contents set directly
  are not drawn); CPU History sets an `NSCollectionViewCompositionalLayout` in code, which is a stub here (it
  places no items; Finch's `NSCollectionViewCompositionalLayout.m` is the candidate). GPU History not tried.
- The inspector: Executable Path empty (Activity Monitor calls `proc_pidpath` itself, which answers on the M1:
  /sbin/launchd, /bin/zsh ...; the path goes into an NSTextView that does not show it: not traced further); the
  Memory pane's rows overlap; Private/Shared memory not computed (libsysmon leaves 53/54 out).
- Energy: the default filter (Applications in last 12 hours) is empty: no history. Energy Impact is an
  estimate (libsysmon above), labelled so here, not a measurement.
- The system's network counters: Packets in, Packets out and Data received are 0 because the kernel's
  `en0` counters are (`ipackets`, `ibytes`, `opackets` 0, `obytes` counted; read with `NET_RT_IFLIST2`
  on the M1). The Wi-Fi receive path is the PCIe Wi-Fi stream's kext (`puredarwin-patches/0001`,
  uncommitted work of that stream) and a fix needs a kernel collection and an SFBOOT install: not done.
- Kind says "Apple" (Apple silicon, as macOS says for a native process) for most processes and
  "Unknown" for some started after Activity Monitor. `sysctl.proc_cputype` answers for every one of 35-37
  processes when asked later (`runs/sf-cocoa3` probe `cputype`); suspected, not established: Activity
  Monitor asked while the process was between fork and exec and keeps the answer.
- The process name column is as wide as the nib says (100 points): long names are cut, as on macOS
  at that width.
- Sample, Spindump, System Diagnostics, Open Files: no tools here; the helper replies with an error.
- Assets.car: images are read since cocoa 0111 (`CPUUsagePill`); vector (PDF) and half-float renditions are not drawn.
- Idle Wake Ups blank on many rows: Activity Monitor leaves the cell empty for these processes; libsysmon gives
  every process its `ri_pkg_idle_wkups` (the rows with figures are the ones with wake-ups); not traced further.
- The Send Signal sheet is a separate panel, not attached to the window.
- (fixed, sixth round: overlay scrollers, cocoa 0114) Hover over an invisible overlay scroller does not reveal
  it (it appears when scrolling or dragging); a faded scroller lets clicks through.
- AM's own CPU at the target's edge: 10.3 % of a core with 42 processes (target "under 10 %").

## The binary
| | |
|---|---|
| Slices | x86_64, arm64e; chained fixups, pointer format 12 |
| Fixups | 7840 rebases (436 authenticated), 1380 binds (1115 authenticated) of 405 imports |
| Libraries | 26 (16 not in the root) |
| Objective-C | 95 classes imported (14 missing), 2177 selectors sent (265 implemented by nobody, a lower bound) |
| Entitlements | `com.apple.sysmond.client` (sysmond refuses clients without it: a probe on the controller Mac got empty tables), `com.apple.activitymonitor-helper`, private AuthorizationServices `com.apple.activitymonitor.kill` |
| Main nib | NIBArchive; the process list is a view-based `NSOutlineView` with 38 archived cell-view nibs (`NSTableViewArchivedReusableViewsKey`), row height 24, intercell 17x0 |
| XPC service | `com.apple.activitymonitor.helper` (`_RoleAccount` root): `sendSignal:toProcessID:`, `sampleProcessWithPID:forSeconds:`, `spindumpWithProcessID:`, `sysdiagnoseWithProcessID:`, `listOpenFilesWithProcessID:` (runs `/usr/sbin/lsof`, `/usr/bin/sample`, `/usr/sbin/spindump`, `/usr/bin/mddiagnose`, libsysdiagnose) |

### Gap table (scanner, `tools/sf_actmon.sh scan`)
Before: `runs/sf-actmon/scan/actmon-before.md` (against Terminal's stage, the M1's root as it is).

| Library | Missing before | How | Real or minimal |
|---|---|---|---|
| libsysmon | 10 (absent) | `libs/sysmon.m`: Finch's model and process table, plus the system-wide table and the attributes below | real |
| CoreServices | 22 | `libs/CoreServices-am.c` in the CoreServices wrapper with Terminal's `compat/CoreServices.c`: Launch Services' process registry answered from the processes (bundle of the executable), Apple Event descriptors | real registry from the processes; sending an Apple Event answers procNotFound |
| NetworkStatistics | 15 (absent) | `libs/NetworkStatistics.c`: a client of the kernel's ntstat control (`com.apple.network.statistics`): every TCP and UDP socket of the system as a source, its process and interface, its bytes and packets | real (layouts from the kernel's `<net/ntstat.h>`, not copied) |
| CoreSymbolication | 14 (absent) | `libs/CoreSymbolication.c` | minimal: used only to symbolicate a sample with a user-chosen dSYM |
| AppKit | 8 | cocoa patch 0100 | NSSearchToolbarItem real; compositional layout objects kept as data |
| AssetCacheServicesExtensions | 4 (absent) | `libs/AssetCacheServicesExtensions.m` | minimal: no content cache |
| libIOReport | 4 (absent) | `libs/IOReport.c` | minimal: no channels |
| IOKit | 3 | `libs/IOKit-am.c` in a wrapper at the framework path re-exporting `/usr/lib/libIOKitCF.dylib`, built by `terminal-app/compat/build.sh` (core: rehearsed, guarded install) | minimal: no power-management daemon, no power source known |
| UniformTypeIdentifiers | 3 (absent) | Finch's UTType (`libs/UniformTypeIdentifiers/`) with our own header | real (Finch's table of system types) |
| CoreFoundation | 2 | `libs/CoreFoundation-am.m` in the CF wrapper: NSCache, `__NSDictionary0__struct` | real |
| Foundation | 2 | `libs/Foundation-am.m` in the Foundation wrapper: NSIndexPath, NSDateComponentsFormatter, some methods libFoundation lacks | real |
| CoreGraphics | 2 | cocoa patch 0102 | minimal: the compositor keeps no "unresponsive" state, sends no CGS notifications |
| Metal | 2 (absent) | `libs/Metal.m` (only these two names) | minimal: no devices |
| libLaunchServicesSupport | 2 (absent) | `libs/LaunchServicesSupport.c` | minimal: no LS server; Activity Monitor falls back to libquit |
| RemoteServiceDiscovery | 2 (absent) | `libs/RemoteServiceDiscovery.c` | minimal: no remote devices |
| libquit | 2 (absent) | `libs/quit.c`, `libs/ActivityMonitorHelper.m` | `LQForceQuit` real (SIGKILL); hang reports go nowhere; the helper service `com.apple.activitymonitor.helper` answered in process: `sendSignal:...` is `kill(2)`, sample/spindump/sysdiagnose/lsof reply with an error (no such tools) |
| libsystemstats | 2 (absent) | `libs/systemstats.c` | minimal: no history |
| SafeEjectGPU | 1 (absent) | `libs/SafeEjectGPU.m` | minimal: no GPU reported |
| libSystem | 1 | `libsystem-default/patches/memorystatus-level.patch` in the default recipe (core: rehearsed, guarded install) | real: the wrapper of system call 453 |
| SecurityFoundation, Suggestions, OpenCL, CrashReporterSupport | 0 (absent) | `libs/empty.c` | empty libraries |

## libsysmon: what each attribute is and where it comes from
Activity Monitor makes two requests (decoded from its calls; names are the keys it stores them under).
Process request (type 1, every 0.5 s, then its update interval):

| Attr | Activity Monitor's name | Source here |
|---|---|---|
| 1, 2, 4, 5, 6, 8, 9, 12, 13, 15, 17, 20 | flags, procStatus, pid, ppid, uid, ruid, rgid, comm, name, pgid, tdev, start | `proc_pidinfo(PROC_PIDTASKALLINFO)` (short form and `kinfo_proc` otherwise) |
| 21 | virtualSize | `pti_virtual_size` |
| 22 | residentSize ("Real Mem") | `proc_pid_rusage` `ri_resident_size` |
| 23, 24 | CPU time (user, system; ns) | `ri_user_time`, `ri_system_time` (Mach time units, converted) |
| 28, 29, 31, 32, 33, 34, 35, 36 | faults, pageIns, msgsSent, msgsRcvd, machCalls, unixSyscalls, contextSwitches, threads | `proc_taskinfo` |
| 44 | arguments | `kern.procargs2` |
| 46 | idleWakeups | `ri_pkg_idle_wkups` |
| 50 | ports | `mach_port_names` count (through `task_for_pid`) |
| 51 | CPU % | difference of 23+24 between two tables of the request over the time between them |
| 66 | anonymousMemory (the "Memory" column, the Memory tab's default sort) | `ri_phys_footprint` |
| 59, 74 | purgeableMemory, compressedMemory ("VM Compressed") | `task_info(TASK_VM_INFO)`: `purgeable_volatile_pmap`, `compressed` |
| 72, 73 | bytesRead, bytesWritten | `ri_diskio_bytesread`, `ri_diskio_byteswritten` |
| 56 | AppSleep | false (there is no App Nap) |
| 52, 62 | powerScore, averagePowerScore (Energy Impact, 12 hr Power) | **an estimate, not measured energy**: the CPU percentage over the interval plus 0.05 points per idle wake-up a second (a wake-up taken as 0.5 ms of a busy core; `ri_pkg_idle_wkups`); 62 the same over the process's life so far (there is no 12-hour history). macOS's comes from the kernel's coalition energy model, which this kernel does not keep |
| 53, 54 | privateMemory, sharedMemory | not computed (a walk of every region of every process): left out |
| 55 | responsible_pid | the process's own pid (the kernel keeps no responsibility; macOS gives the same for a process nobody launched on another's behalf). Left out, the Energy tab ended in a CoreFoundation halt (`objectForKey:` nil) |

System request (type 2, every 1 s): 4 wired pages, 14 purgeable, 20 compressor, 33 external,
34 internal, 38 pages in use (active+inactive+speculative+throttled+wired+compressor), 36 total
pages (`hw.memsize`), 24 threads (the sum of every process's `pti_threadnum`: `kern.num_threads` is the limit, 15360 on the M1), 25..28 disk operations and bytes (the block
storage drivers' `Statistics`), 29..32 packets and bytes in and out (`NET_RT_IFLIST2`, not lo0).
Activity Monitor computes: Memory Used = (38 - 33) pages, App Memory = 34 - 14, Cached Files =
33 + 14, Wired = 4, Compressed = 20, Swap from `vm.swapusage`, pressure from `memorystatus_get_level`;
CPU load from `host_processor_info` itself.

## Taken from Finch
Finch (`sources/finch`, "MIT OR Apache-2.0"), commit `2d9b1bb`:

| Finch file | Where here | What |
|---|---|---|
| `userland/libsysmon/sysmon.m` | `activity-monitor/libs/sysmon.m` (Finch's notice at the top, our changes listed under it) | the request/table/row model (attribute bit arrays, values in an XPC array in attribute order, interval rounding, the appliers), the in-process process table from libproc and sysctl and its first attributes. Not taken: its OS_object layouts (this libSystem has no `_os_object_alloc`; our objects are Objective-C objects) |
| `userland/CoreUI/CUIStore.c` (whole), `CUIRendition.c` (`cui_csi_parse`, `cui_csi_tlv`, `cui_csi_color`), `CUIPrivate.h` | cocoa patch 0105: `AppKit/PDCUIStore.m`, `PDCUIRendition.m`, `PDCUIPrivate.h` (notices kept) | the compiled asset catalog reader (BOM store, trees, facets, renditions) and colour renditions, for `+[NSColor colorNamed:]` |
| `userland/CoreAutoLayout/NSLayoutConstraint.m` (`-constant`), `FinchLayoutItem.h` | cocoa 0106: `AppKit/NSLayoutConstraint.m` (a comment names the source) | the values of a nib's standard space: 20 points to the superview, 8 between siblings, and the rule which applies (no code taken) |
| `userland/Foundation/NSDecimal.m` (whole), `NSPointerCollections.m` (to its C API), `NSForSwiftFoundation.m` (NSPersonNameComponents), `NSLocalizedFormatters.m` (NSPersonNameComponentsFormatter) | libFoundation (`terminal-app/patches/foundation-macos-applications.patch`: `Numeric.subproj/NSDecimal.m`, `Collections.subproj/NSPointerCollections.m`, `Formatters.subproj/NSPersonNameComponents.m`) | NSDecimalNumber and the NSDecimal functions; NSHashTable/NSMapTable object API (categories on PureDarwin's classes, whose C API stays), NSPointerArray, NSPointerFunctions; the name components and their formatter (sixth round) |
| `userland/AppKit/NSCollectionView.m`, `NSCollectionViewLayout.m` | cocoa 0110 (`NSCollectionView/`) | the modern collection view, flow and grid layouts; a nib or class of the identifier's name when nothing is registered, the grid's archived settings read (ours) |
| `userland/CoreUI/CUIRendition.c` (bitmaps, raw data, links), `CUIDeepmap.c` (whole), `CUICatalog.m` (rendition choice, image from a rendition) | cocoa 0111 (`PDCUIRendition.m`, `PDCUIDeepmap.m`, `PDCUIImage.m`) | images of an application's asset catalog for `+imageNamed:`; libcompression and zlib looked up at run time |
| `userland/AppKit/NSSystemImages.m` (drawing helpers, symbol glyph table, name parser) | cocoa 0111 (`PDSymbolGlyphs.m`) | about 150 symbol names drawn in code (Terminal's Preferences toolbar: gearshape, doc with a badge, globe); filled enclosures draw wrongly (Onyx2D's transparency layer) |
| `userland/CoreAutoLayout/FinchLayoutSolver.c` | not taken | measured as the core under our whole-tree driver: Terminal's Preferences took over 10 s of solving (104 solves at 9 s) against 1.57 s (206 solves) with ours; it needs the incremental driver to pay off |
| `userland/UniformTypeIdentifiers/UTType.m`, `UTCoreTypes.m`, `UTAdditions.m`, `UTTypeTable.inc` | `activity-monitor/libs/UniformTypeIdentifiers/` (unchanged, a line saying so under the SPDX line) | UTType and the system's declared types; the header (`include/`) is ours, from the names |

## Cocoa patches (0100..0114; 0108..0114 the sixth round, regenerated by the same script from `am-0107`, `am-0109`, `am`)
Since the fifth round they are `integrations/puredarwin/cocoa/patches/0100..0107` in `cocoa/patched.nix` (0102 on
CoreGraphics), regenerated by `cocoa/patches/mkpatches-0100.sh` from `runs/sf-actmon/pw` (`base`, `am-0105`, `am-0106`,
`am`). The history of the night:
Rebased on 14 November (~13:20Z) over TERM's committed 0051..0059: they apply after 0059 and, applied to
a fresh copy of that tree, reproduce ACTMON's tree exactly. Dropped in the rebase: 0104's value
transformer by class name in NSBinder.m (TERM's 0059 does the same). Checked on the M1 with the rebased
build: sorting by Process Name, the Memory tab, search (`shots/rebased.png`); Terminal.app's Preferences
and Inspector with 0103+0104 (`shots/tPrefs.png`, `tInsp.png`).

Made in ACTMON's own copy of the patchwork tree (`runs/sf-actmon/pw`: `base/` = TERM's committed
series, `am/` = with ours), `cocoa-patches/mkpatches.sh` writes them; test builds
`runs/sf-actmon/appkit-dev.sh`, `cg-dev.sh` (stage only; not installed on the disk).

| Patch | What |
|---|---|
| 0100 | AppKit: `NSWorkspaceDidWakeNotification`, NSSearchToolbarItem, the compositional layout's description objects (kept as data), views' `identifier`; `+[NSColor colorNamed:]` (an application's asset catalog is not read: a name that says its colour gets that system colour), `systemCyanColor`, `monospacedDigitSystemFontOfSize:weight:`, `-[NSBundle imageForResource:]`, `-[NSSavePanel setAllowedContentTypes:]`, `-[NSPrintOperation runOperationModalForWindow:...]`, `-[NSTouchBar setItemIdentifiers:]` |
| 0101 | AppKit: view-based NSTableView/NSOutlineView (NSTableCellView, NSTableRowView, `makeViewWithIdentifier:owner:`, the table's archived cell-view nibs, row views laid out before each display, reuse), hidden table columns (`NSHidden`, no space in table or header), the header's sort indicator; a header click asks the delegate `…shouldSelectTableColumn:` and tells it `…didClickTableColumn:` (row clicks do not ask), the table at least as wide as its clip view with the last column re-fitted |
| 0102 | CoreGraphics: `CGSEventIsAppUnresponsive`, `CGSRegisterNotifyProc` |
| 0103 | AppKit: without EGL a layer-backed view is drawn as an ordinary view (no layer context) |
| 0104 | AppKit: NIBArchive booleans (4 false, 5 true), a cell's three line-break bits, custom views' constraints and identifier, value transformers named by class, constrained labels re-measured when the shown string's length changes (on the main thread), bindings apply changes made off the main thread on it, the window menu strip also on full-size-content windows |
| 0105 | AppKit: an application's named colours from its `Assets.car`, with Finch's catalog reader (`PDCUIStore.m`, `PDCUIRendition.m`, `PDCUIPrivate.h`) |
| 0106 | AppKit: a nib's standard-space constraints, archived hugging and compression priorities, NSMatrix's intrinsic size, the table header as wide as its clip view without a vertical scroller; `PD_LAYOUT_TRACE=1` (the fault that kept the merge out: Terminal's panes) |
| 0108 | AppKit: NSMenuItem's object getters retained and autoreleased (with libFoundation's CFAutorelease fix) |
| 0109 | CoreGraphics: ImageIO's thumbnail call and image metadata (Finch's Contacts and MediaAccessibility) |
| 0110 | AppKit: Finch's modern NSCollectionView, flow and grid layouts; `-[NSViewController isViewLoaded]`; `PD_CV_TRACE=1` |
| 0111 | AppKit: asset-catalog images (Finch's CoreUI decoders and rendition choice), Finch's symbol glyphs |
| 0112 | AppKit: `PD_SOLVE_TIME=1` (each constraint solve's time and the sum) |
| 0113 | AppKit: macOS 26 toolbar buttons (glyphs in one capsule, one height, even spacing, chevron); a preferences toolbar's selected item in a soft rounded rectangle |
| 0115 | AppKit: `-[NSEvent CGEvent]` autoreleased with -autorelease (CoreFoundation's CFAutorelease leaves it) |
| 0114 | AppKit: overlay scrollers (legacy with `AppleShowScrollBars=Always` or `PD_SCROLLERS=legacy`); the legacy corner painted |
| 0107 | AppKit: symbol stand-ins, toolbar item placement and margins, hidden menus out of the strip, image cell value, column widths kept, stripes and header past the columns and rows, view-based rows reloaded in place, catalog colours resolved once, a pop-up with items has one selected, PD_CA_TRACE read once |

## Tools
- `tools/sf_actmon.sh libs | stage | push | run | scan | probe`
- `PD_LAYOUT_TRACE=1` (cocoa 0106): every constraint of each layout and every frame it sets, on
  standard error; with the nib's archived frames it showed what the solver got wrong in Terminal's panes.
- `PDTableDebug=1` (cocoa 0101): a view-based table's rows, visible range and frame at each layout.
- Scratch, not committed: `runs/sf-am3/` (`amopen.sh`: end Activity Monitor and start the installed one
  with `open -a` and the session's environment only; `amclick.sh`/`locate.py`: find the window in a
  capture by its title and click relative to it; `cpu.sh`: CPU seconds over an interval).
- `probe/sf-am-probe.c`: what the kernel answers to each call libsysmon makes
- `runs/sf-actmon/tools/nibdump.py` (scratch, not committed): a NIBArchive reader for looking at the nibs
