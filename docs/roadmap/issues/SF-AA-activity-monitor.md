# SF-AA: Apple's Activity Monitor on SiliconForest (GitHub issue 53)

## Goal
The unmodified Activity Monitor.app (macOS 26.5.2 copy from the controller Mac, version 10.14,
`private/mac-apps/m5-26.5.2/Activity Monitor.app`, git-ignored) runs on the SiliconForest desktop of
the 13" M1 MacBook Pro and shows SiliconForest's real processes with real CPU and memory figures.
The binary is never committed. Method and tools are Terminal.app's (SF-Y,
`docs/roadmap/issues/SF-Y-terminal-app.md`); everything here is in `integrations/puredarwin/activity-monitor/`.

## State
(kept current below; the newest round first)

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
| IOKit | 3 | `libs/IOKit-am.c`, a wrapper at the framework path re-exporting `/usr/lib/libIOKitCF.dylib` | minimal: no power-management daemon, no power source known |
| UniformTypeIdentifiers | 3 (absent) | Finch's UTType (`libs/UniformTypeIdentifiers/`) with our own header | real (Finch's table of system types) |
| CoreFoundation | 2 | `libs/CoreFoundation-am.m` in the CF wrapper: NSCache, `__NSDictionary0__struct` | real |
| Foundation | 2 | `libs/Foundation-am.m` in the Foundation wrapper: NSIndexPath, NSDateComponentsFormatter, some methods libFoundation lacks | real |
| CoreGraphics | 2 | cocoa patch 0102 | minimal: the compositor keeps no "unresponsive" state, sends no CGS notifications |
| Metal | 2 (absent) | `libs/Metal.m` (only these two names) | minimal: no devices |
| libLaunchServicesSupport | 2 (absent) | `libs/LaunchServicesSupport.c` | minimal: no LS server; Activity Monitor falls back to libquit |
| RemoteServiceDiscovery | 2 (absent) | `libs/RemoteServiceDiscovery.c` | minimal: no remote devices |
| libquit | 2 (absent) | `libs/quit.c` | `LQForceQuit` real (SIGKILL); hang reports go nowhere |
| libsystemstats | 2 (absent) | `libs/systemstats.c` | minimal: no history |
| SafeEjectGPU | 1 (absent) | `libs/SafeEjectGPU.m` | minimal: no GPU reported |
| libSystem | 1 | `libsystem-memorystatus-level.patch` (`build-libsystem-am.nix`) | real: the wrapper of system call 453 |
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
| 52, 62 | powerScore, averagePowerScore | not known here: left out |
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
| `userland/UniformTypeIdentifiers/UTType.m`, `UTCoreTypes.m`, `UTAdditions.m`, `UTTypeTable.inc` | `activity-monitor/libs/UniformTypeIdentifiers/` (unchanged, a line saying so under the SPDX line) | UTType and the system's declared types; the header (`include/`) is ours, from the names |

## Cocoa patches (ACTMON's numbers 0100..0149)
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

## Tools
- `tools/sf_actmon.sh libs | stage | push | run | scan | probe`
- `probe/sf-am-probe.c`: what the kernel answers to each call libsysmon makes
- `runs/sf-actmon/tools/nibdump.py` (scratch, not committed): a NIBArchive reader for looking at the nibs
