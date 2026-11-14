# SF-AA: Apple's Activity Monitor on SiliconForest (GitHub issue 53)

## Goal
The unmodified Activity Monitor.app (macOS 26.5.2 copy from the controller Mac, version 10.14,
`private/mac-apps/m5-26.5.2/Activity Monitor.app`, git-ignored) runs on the SiliconForest desktop of
the 13" M1 MacBook Pro and shows SiliconForest's real processes with real CPU and memory figures.
The binary is never committed. Method and tools are Terminal.app's (SF-Y,
`docs/roadmap/issues/SF-Y-terminal-app.md`); everything here is in `integrations/puredarwin/activity-monitor/`.

## State
(kept current below; the newest round first)

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
| 55 | responsible_pid | not known here: left out |

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
| `userland/UniformTypeIdentifiers/UTType.m`, `UTCoreTypes.m`, `UTAdditions.m`, `UTTypeTable.inc` | `activity-monitor/libs/UniformTypeIdentifiers/` (unchanged, a line saying so under the SPDX line) | UTType and the system's declared types; the header (`include/`) is ours, from the names |

## Cocoa patches (ACTMON's numbers 0100..0149)
Made in ACTMON's own copy of the patchwork tree (`runs/sf-actmon/pw`: `base/` = TERM's committed
series, `am/` = with ours), `cocoa-patches/mkpatches.sh` writes them; test builds
`runs/sf-actmon/appkit-dev.sh`, `cg-dev.sh` (stage only; not installed on the disk).

| Patch | What |
|---|---|
| 0100 | AppKit: `NSWorkspaceDidWakeNotification`, NSSearchToolbarItem, the compositional layout's description objects (kept as data), views' `identifier`; `+[NSColor colorNamed:]` (an application's asset catalog is not read: a name that says its colour gets that system colour), `systemCyanColor`, `monospacedDigitSystemFontOfSize:weight:`, `-[NSBundle imageForResource:]`, `-[NSSavePanel setAllowedContentTypes:]`, `-[NSPrintOperation runOperationModalForWindow:...]`, `-[NSTouchBar setItemIdentifiers:]` |
| 0101 | AppKit: view-based NSTableView/NSOutlineView (NSTableCellView, NSTableRowView, `makeViewWithIdentifier:owner:`, the table's archived cell-view nibs, row views laid out before each display, reuse), hidden table columns (`NSHidden`, no space in table or header), the header's sort indicator |
| 0102 | CoreGraphics: `CGSEventIsAppUnresponsive`, `CGSRegisterNotifyProc` |

## Tools
- `tools/sf_actmon.sh libs | stage | push | run | scan | probe`
- `probe/sf-am-probe.c`: what the kernel answers to each call libsysmon makes
- `runs/sf-actmon/tools/nibdump.py` (scratch, not committed): a NIBArchive reader for looking at the nibs
