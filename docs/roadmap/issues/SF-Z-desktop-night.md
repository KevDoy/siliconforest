# SF-Z: the desktop of the night of 14 November (agent DESK)

The owner's words: "our spaces functionality in chonkstep needs to work as well as screen capture and
screenshots … keyboard command combinations to work just as they do in Linux: command-q, command-w,
the screenshot command etc., but the gesture to go to other spaces needs to be ported in to this
version, and we want to start the chonkstep dock that has a few items on it; it's a separate program
but get it to be part of the default install."

**Truthfulness.** Everything below marked "observed on the M1" ran on the M1's SiliconForest disk root
with **synthetic input** from the compositor's inject module (test build `.next`), and was judged from
the compositor's own captures of its frames and its log. Nobody looked at the panel, no real finger
touched the trackpad and no real key was pressed.

## What a person at the M1 can now do (and could not before)
- Command-1 … Command-0: go to Space 1 … 10; Command-Shift-<n>: take the focused window there;
  Command-Option-Shift-<n>: send it without following; Command-Tab / Command-Shift-Tab: next/previous
  occupied Space; a notice says "Space N". Command-O: Overview with the Spaces row.
- Three- or four-finger swipe left/right: next/previous Space, the desk following the fingers.
  The trackpad driver now hands such swipes to the compositor (new kernel collection, installed
  14 November ~10:53Z). **Proven only with constructed trackpad reports fed to the driver's decoder,
  not with real fingers** — see "The gesture". Control-Command + two-finger scroll does the same
  (stand-in, kept).
- Command-W closes the window, Command-Q quits the application (all its windows). With a Mac
  application focused (Terminal.app, Activity Monitor) its Command chords are its own, as on macOS
  (Command-W/Q/F/T/S/O/1..9/=/- …), except the desk's: Command-Space, Command-(Shift/Control-)Tab,
  Command-Shift-<digit>, Command-Option-Shift-<digit>, Command-Control-Shift-3/4/5 (from desk-010:
  build 009 still took Command-F/T/S/O/digits from Mac applications).
- Command-Control-Shift-3: screenshot of the screen, Command-Control-Shift-4: drag an area,
  Command-Control-Shift-5: the focused window. Saved as `~/Pictures/Screenshots/Screenshot
  <date> at <time>.png` (owner-only) and put on the clipboard (`image/png`); a notice says so.
  `chonkstep-darwin-desktop --screencapture [-c] [-w | -R x,y,w,h] [file]` from a shell does the same.
- The Chonkstep dock: on the right its column (clock, network, CPU/memory, sound, link, Bluetooth,
  power, the Clip), on the left the launcher strip with Terminal, Terminal.app and Activity Monitor
  (when its bundle is installed). A running application's tile has the red dot; a click on it
  brings its window forward (on its Space), on a stopped one starts it.
- Mac applications can go full screen, maximise, minimise and bring their own windows forward
  (`xdg_toplevel` requests and `xdg_activation_v1`; for agent TERM's Terminal.app work).

Captures (copies in `runs/m1-desktop-010/desk-tests/shots/`, made by the compositor, 2560x1600):
`t6-a-space1.png` (dock, two terminals), `t6-b-space2.png` (empty Space 2), `t6-c-carried3.png`
(window carried to Space 3), `t6-d-swipe-mid.png` (a swipe held: the Space slides with the fingers),
`t6-e-swipe-done.png`, `t6-f-swipe-back.png`, `t6-g-overview.png` (Overview, Spaces 1 2 3),
`t6-h-after-shot.png`, `t6-cli.png` (the PNG the command line wrote), `t6-i-dock-terminal.png`
(dock tile click -> the terminal on Space 3), `t6-j-cmdw.png` (Command-W closed it), `t7-a-dock.png`
(the dock column inside the panel, the windows out of its zone).

## 1. Key bindings
The owner's Linux session (`~/.config/chonkstep/config.toml`): `desktop = "omarchy"` (the Omarchy
keymap, `wm-config/src/preset.rs`, 163 chords), `interaction_mode = "spaces"`, `keyboard_mode =
"desktop"`, plus `"super+o" = "overview"`. On the M1 the Command key is Super (`Modifiers::SUPER`,
HID 0xE3 -> evdev 125). The standalone desk (`chonk-shell/src/startup.rs`
`standalone_darwin_keybindings`) takes that table, keeps what the standalone desk can carry out
(no `run <command>`: Omarchy's programs do not exist here; programs start through the desktop's
own launcher), adds the owner's override, Command-Space for the desktop menu, and the Mac profile's
global chords (`MAC_BINDINGS`) only where the Omarchy table leaves the chord free. 69 chords; the
full list is printed by `cargo test -p chonk-shell standalone_key_table_dump -- --ignored --nocapture`.

| Linux chord (Omarchy keymap / Mac profile) | Action | On the M1 | State |
|---|---|---|---|
| Super-Return | terminal | Command-Return | works (desktop's terminal; from M1 runs of earlier days, not re-run tonight) |
| Super-W | close window | Command-W | **observed** (t6-j); Mac app: goes to the app (not exercised) |
| Cmd-Q (Mac profile) | quit application | Command-Q | bound; native: wm-core quit_application (closes every window of the app); Mac app: goes to the app (since 13 Nov). Native path not exercised tonight |
| Super-Ctrl-Shift-3 | capture screen (file + clipboard) | Command-Control-Shift-3 | **observed** (file 219,906 bytes, 206 ms) |
| Super-Ctrl-Shift-4 | capture area | Command-Control-Shift-4 | bound; drag-select with an outline, Escape/right click cancels. Not exercised on the M1 |
| Super-Ctrl-Shift-5 | capture toolbar | Command-Control-Shift-5 | **not done** as a toolbar: takes the focused window (one of the toolbar's modes). Not exercised |
| Print / Super-Print / Alt-Print | omarchy screenshot / colour picker / screen recording | — | not applicable (no Print key; programs absent); recording not done |
| Super-Ctrl-Escape | stop recording | — | not done (no recording) |
| Super-1..0 | Space 1..10 | Command-1..0 | **observed** (Command-2, Command-1) |
| Super-Shift-1..0 | carry window to Space | Command-Shift-1..0 | **observed** (Command-Shift-3) |
| Super-Shift-Alt-1..0 | send window to Space | Command-Option-Shift-1..0 | bound, not exercised |
| Super-Tab / Super-Shift-Tab / Super-Ctrl-Tab | next / previous occupied Space, last Space | Command-(Shift/Control)-Tab | bound, not exercised |
| Super-O (owner's override) | Overview | Command-O | **observed** (t6-g) |
| Super-F / Super-Alt-F / Super-Ctrl-F | full screen / maximise / tiled full screen | Command-F, Command-Option-F, Command-Control-F | bound, not exercised |
| Super-arrows / Super-Shift-arrows | focus / move | Command-arrows / Command-Shift-arrows | bound, not exercised |
| Super-=/-, Super-Shift-=/- | grow/shrink width/height | same with Command | bound, not exercised |
| Super-T, Super-L, Super-Shift-L | floating, layout (Mosaic/Flow), Freeform | same with Command | bound, not exercised |
| Super-S / Super-Alt-S | scratchpad | same with Command | bound, not exercised |
| Ctrl-Escape | window menu | Control-Escape | bound, not exercised |
| Super-Space | Omarchy launcher | Command-Space: the desktop menu (Applications, Terminal, Theme, …) | works (since 11 Nov) |
| Cmd-M / Cmd-Opt-M / Cmd-H / Cmd-Opt-H / Cmd-grave (Mac profile) | miniaturize, … | same | bound; Mac apps get them themselves; not exercised |
| Alt-Tab | window switcher | Option-Tab | wm-core's built-in grab, not exercised |
| Super-Ctrl-Q, Super-Escape, Super-Ctrl-C/O/H/S/E/…, notifications, reminders, night light | Omarchy programs | — | not applicable (programs absent) |
| Super-Alt-Home/Super-Home, Super-Slash, monitors | Omarchy scripts, monitors | — | not applicable |
| Volume / brightness keys | | F1/F2, F10–F12, Touch Bar | work as before (compositor's own) |

Mac applications: see the first section (from desk-010). Observed with build 009 on the M1: with
Terminal.app focused, Command-F made the window full screen on a Space of its own (wm-core's Spaces
rule) and Terminal.app answered with unset_fullscreen (`WMSTATE request … Remove Fullscreen`): the
desk had taken Command-F from it. Command-Q went to Terminal.app and it quit (its window went).
With desk-010 (observed): Terminal.app focused, Command-O did not open the desk's Overview (it went to
the application), Command-Shift-2 carried Terminal.app's window to Space 2 (the desk's), Command-1 went
to Terminal.app (its window 1) and did not switch Space: from a Mac application, Spaces are reached
by swipe, Command-Tab or Command-Shift-<digit> (carrying), not by Command-<digit>.

## 2. Spaces
wm-core's Spaces profile (`InteractionMode::Spaces`, keyboard Desktop) on the standalone desk; the
backend now admits it (`CpuBackend::supports_mac_interaction`, which the shell requires for both
Spaces and Mac keys; the Mac keyboard translation itself is not selected). Observed: switching,
carrying (a new Space 3 was made), the Overview row with three Spaces, windows staying on their
Space, a new window opening on the current one (the dock's terminal tile and startup terminals).
Mac-application windows follow the same rules (they are ordinary toplevels; popups and sheets are
drawn with their parent) — **not exercised with a Mac application tonight**.
Indicator: a notice "Space N" at each change (the desk has no bar); the dock's Clip shows the
Space only through ChonkStep's control socket, which this desk does not have, so it stays at "1".

## 3. The gesture
**Compositor (done, observed with synthetic records):** `crates/wm-darwin/src/gesture.rs`. Three- and
four-finger swipes use wm-core's own recognizer (`SwipeTracker`: axis lock at 12 units, 1.25
dominance, timed velocity samples) and physics (`gesture_physics`: resisted edges, velocity
projection, critically damped spring), the same code the Linux session runs; distance 80 (span 160
units, the Linux default). Left (fingers moving left) = next Space, right = previous; the current
Space slides with the fingers and its neighbour comes in beside it (scene `Slide`), released before
halfway it springs back, beyond (or flicked) it goes; a missing neighbour resists. Vertical: up opens
Overview, down closes it (at release; the Linux morph is not ported). Natural scrolling does not
turn it (physical directions, as on Linux). Two-finger swipes stay with scrolling.
Evidence: injected `swipe begin/update/end 3 ...` records: `WMGESTURE release position=1.074
velocity=1.646 target=1` -> Space 2, back with `position=-1.132 target=-1`, mid-swipe capture t6-d.
**Trackpad driver:** the kext (`m1-trackpad-child/multitouch`, PDM1SPIHID in multitouch mode)
decoded every contact but handed userspace only a synthesized pointer (buttons, motion, one vertical
wheel byte); three and four fingers were recognised by its libinput port and dropped
(`M1TP_MT_G_IGNORED`). Now it gives them to the compositor: see "Kernel" below.
**Stand-in (labelled so):** Control-Command with a two-finger scroll drives the same swipe (each
wheel step = 24 units of horizontal travel, scrolling down = next Space, released 250 ms after the last
step). Not exercised on the M1.

## 4. Screen capture and screenshots (installed build)
`crates/wm-darwin/src/capture.rs`. The picture is the CPU render of the scene without the pointer.
Keys as in the table; files `~/Pictures/Screenshots/Screenshot <date> at <time>.png` (`HOME` of the
session: `/var/root`), mode 0600, `O_EXCL` (never replacing); clipboard: the compositor serves
`image/png` itself (written from a thread). Command line: `chonkstep-darwin-desktop --screencapture`
(observed: `/private/tmp/desk-cli.png 2560x1600`, 184 ms).
**Not a spying channel:** in the installed build nothing reads a request from a file. A capture is
started only by (1) the compositor's own key binding, or (2) a connection to the socket
`$XDG_RUNTIME_DIR/capture` (`/private/var/run/gpu-desktop/capture`): the directory is the session's
(0700, root), the socket 0600, and the compositor serves a connection only when the peer's effective
uid (`getpeereid`) is its own (root): same user only, no other account. The test-only inject module
(file requests and captures) is compiled only with the feature `test-inject`; the installed
`/usr/bin/chonkstep-darwin-desktop` has no inject code (`WMINJECT absent` string, no `WMINJECT armed`).
Not done: recording, the capture toolbar, a selection outline that dims the rest. Seen in the
captures: the one-line notices ("Space 2", "Screenshot saved: …") are cut short ("S…"): the notice is
sized by a font measure smaller than the one it is drawn with at 200 % (chonk-shell `show_notice`); not
fixed.

## 5. The dock
The real Chonkstep dock (`dock/crates/chonk-dock`, built for Darwin by
`integrations/puredarwin/desktop/tools/build_dock.sh`; Darwin needed only socket shims in
`chonk-dock-proto`: arc4random, fcntl instead of SOCK_CLOEXEC/NONBLOCK, getpeereid/LOCAL_PEERPID).
The compositor gained what it asks of a compositor: `zwlr_layer_shell_v1` (`layers.rs`: anchors,
margins, exclusive zone -> wm-core's work area, pointer, popups of a layer), `zwlr_foreign_toplevel_
management_v1` (`foreign.rs`: titles, app ids, activated/minimized/maximized/full screen; activate,
close, (un)minimize, (un)maximize, (un)fullscreen) and `zxdg_output_manager_v1`. Host test with the
real dock: `tests/dock_host.rs` (its column, Clip and launcher mapped, exclusive zone applied).
Started by `sf-session` once the compositor's socket exists (and again at every compositor start),
log `/var/log/chonkstep-dock.log`. Items (`integrations/puredarwin/desktop/system/`):
`org.siliconforest.terminal` (Exec `chonkstep-darwin-desktop --desktop-request terminal`: the
desktop's own launcher, through the same uid-checked socket), `com.apple.Terminal` (`open -a
Terminal`), `com.apple.ActivityMonitor` (`open -a "Activity Monitor"`, TryExec on
`/System/Applications/Utilities/...`: absent on the M1 tonight, so not shown). The Linux dock has no
default pins; these are this desk's. Observed: dock up, running indicator on Terminal, click ->
the terminal brought forward on its Space. A full-screen window covers the dock (desk-011, observed with
Command-F on a desktop terminal: `WMLAYER top layer covered`, capture `t11-a-fullscreen.png`).
Not observed: launching Terminal.app or a new desktop terminal from the dock; the dockapps (SOCK_SEQPACKET is not on Darwin: "dockapps are unavailable this
session"); the instruments' data (Linux samplers; most show nothing here). The Clip is placed by the
dock itself and sits partly below the panel's edge at 200 %.

## 6. Default install, records
- Patches: `integrations/puredarwin/terminal-app/compositor/wm-darwin-client-states.patch` (6) and
  `wm-darwin-desktop-night.patch` (7) on top of 1-5; applied to a copy of `wm-darwin-cmdq-src` they give
  the build source exactly (checked with `diff -r`). README there updated.
- Disk-root install: `m1-storage/system/sf-install.sh` installs the dock, its items and pins when
  the boot carries them; `m1-storage/tools/dr_desk_boot.sh` `DR_DOCK=<file>` carries them;
  `desktop/tools/desk_m1.sh dock <file>` puts them on a running system over SSH.
  RAM-root desktop wrappers (`tools/gpu_desktop.sh`, `ds_desktop_boot.sh`, …): **not changed** (bench
  tools; their guest scripts would have to start the dock).
- Session (`m1-storage/system/sf-session`, on the M1 since boot dr60): restart of the compositor
  without a reboot (`relaunch` / `relaunch-next` in `$XDG_RUNTIME_DIR/session-end-request`, used by
  `desk_m1.sh relaunch`), the dock, `/var/log/desktop-session.log`, and the fallback to
  `/usr/bin/chonkstep-darwin-desktop.safe` when a compositor ends with an error within 20 s.
- On the M1: see "Final state" below for the compositor builds. `.safe` = desk-daily-001 `b1104b75…`; before: `.before-desk`
  (cmdq-daily-001, `894bbf15…`), `.before-desk001`, `.before-desk006`, `.next.before-desk*`;
  `/usr/bin/chonkstep-dock` (`5f1baa06…`); `/usr/share/applications/*.desktop` (3);
  `/private/var/db/desktop-state/chonkstep/dock` (pins); `/usr/libexec/sf-session` (14653 bytes; the
  ones of before: `sf-session.before-desk`, `.before-apps`, `.old`).
  Revert: `ln -f <path>.before-desk <path>.back && mv -f <path>.back <path>` for the compositor and
  `.next`; `ln -f /usr/libexec/sf-session.before-desk ...` likewise; remove `/usr/bin/chonkstep-dock`
  (the session then starts no dock), the three `.desktop` files and the pins file.
- Cargo tests (host, `XDG_STATE_HOME` at a scratch directory; full run on the final source, that of
  desk-011, results in `runs/m1-desktop-010/desk-tests/cargo-test-011.txt`; lib and dock_host also on
  every build in between): `cargo test --offline --locked -p wm-darwin
  --features desktop-shell`: lib 129/129; wayland_dmabuf 5/5; wayland_theme 3/3; wayland_shm 9/10
  (`autopilot_exercises_the_whole_desktop_against_a_real_window` fails); wayland_scale 3/4
  (`a_window_too_large_for_the_panel_at_a_scale_is_shrunk_and_gets_its_size_back` fails). **Both
  failures are the same on the base source** (`wm-darwin-cmdq-src`, run tonight; there the lib test
  `the_desk_is_drawn_at_every_scale_and_never_stretched` also failed: its expected menu lacked the
  "Session" row of 96effcf; the expectation is corrected). dock_host 1/1 (ignored by default; needs a
  host build of the dock). chonk-shell: `standalone_darwin_key_tests` 1/1 (the whole chonk-shell suite
  was not run).

## Kernel: swipes from the trackpad driver (after the coordinator put it in scope)
**Why this shape (the smallest change).** The record that reaches `/dev/usb_hid_mouse`
(PDHIDEventQueue in IOHIDFamily: sequence, buttons, dx, dy, one wheel byte, absolute flag) has no
room for a swipe, and IOHIDFamily is a hash-pinned revision (`m1-native-input/revisions/002`). So
only the trackpad kext changed: its decoder emits libinput's swipe gesture beside the pointer
report, and the kext keeps the last 32 swipe events in a ring that userspace reads with sysctl. No
IOHIDFamily, no new device node, nothing of `pd_nvme_wgate.c/.h`, the fence constants or
`pd_halt_gate.h`.
- `m1tp_mt.c`: three or four active fingers -> `M1TP_MT_G_SWIPE`; BEGIN, UPDATE (the average
  travel of the touches, accelerated with the pointer's libinput curve, in libinput's units: what
  a Linux compositor gets as the swipe's dx/dy, physical directions) and END when the fingers leave
  (a lifted or added finger debounced 100 ms, as libinput does for gestures). Host tests
  (`run-host-tests.sh`): all earlier checks pass; three and four fingers 40 mm left in 300 ms give
  1 begin, 40 updates, 1 end, travel -419 units, no pointer motion; two fingers no swipe.
- `PDM1SPIHIDMultitouch.inc`: the ring, `sysctl kern.pd_trackpad_swipe` (opaque, read-only:
  version, next sequence, 32 events of 16 bytes) and `kern.pd_trackpad_swipe_test` (root writes
  3/4 = left, -3/-4 = right: a constructed swipe, report bytes built by `m1tp_mt_build`, through
  `m1tp_mt_parse` and `m1tp_mt_step` on a scratch state, 8 ms apart in real time; marked synthetic).
- Compositor (patch 8): reads the ring once a frame (`Server::trackpad_swipes`), each new event a
  Swipe as in section 3; a kernel without the sysctl is noticed once and left alone.
- Collection: `m1-storage/build-kc-native-rw-halt.nix` (unchanged recipe; the kext comes from
  `m1-touchbar/build-spihid.nix` <- `m1-trackpad-child/multitouch`). Kit:
  `m1-native-boot/install/build.sh runs/m1-desktop-010/desk-kit-001`: `kernel.bin` `821cb932…`
  (10,165,616 bytes), `boot.bin` `81d58f1b…` (unchanged). Control: the same kit built with the
  three changed files put back gives `093b61f1…`, the installed kernel of before, byte for byte.
- Install (from Linux, inside gui.lock): `e2fsck -fn` of the partition (`dr_linux_root.sh check`):
  rc 0, clean; `sf-bootbin-install.sh <PARTUUID of SFBOOT> boot.bin kernel.bin`: `sf/kernel.bin`
  `821cb932…`, `sf/kernel.bin.old` `093b61f1…`, boot.bin left alone; `asahi-bless --next`; the
  first boot came up (sysctl present, length 520, `PDM1SPIHID: MT stage=config enabled=1`); a
  second boot after Linux too. `e2fsck -fn` after: rc 0, clean. Records: `install/out/` holds the new
  kit (the one of before in `runs/m1-desktop-010/install-out-before-desk-swipes/`), SF-R-storage.md.
  Rollback: from Linux `sudo bash ./sf-bootbin-install.sh <PARTUUID of SFBOOT> --rollback`.
- **Observed on the M1 (constructed reports, not fingers):** `sysctl -w kern.pd_trackpad_swipe_test=3`
  -> kernel log `MT stage=swipe-begin fingers=3 synthetic=1`, `swipe-test … direction=left
  next_seq=43`; compositor `WMGESTURE trackpad swipes: first event phase=Begin fingers=3
  synthetic=1`, `release position=1.162 … target=1`, `WMSPACE current=2`; `=-3` -> `position=-1.162
  velocity=-3.824 target=-1`, back to Space 1. Capture `k-b-kmid.png`: the Space sliding with the
  constructed fingers. Not tested: real fingers, the controller's real reports of three or four
  fingers (the decoder's handling of them is the libinput port that has driven the pointer and
  scrolling since boot mt01/tp02), a palm or thumb among them, natural feel.
- **For the owner, in the morning:** on the desk, swipe three fingers to the left on the trackpad:
  the desk should slide to the next Space (a new one if this one has windows), three fingers right:
  back. If nothing moves, `ssh root@<M1> 'sysctl -w kern.pd_trackpad_swipe_test=3'` shows whether
  the path behind the fingers works.

## Final state (11:35Z)
(Later the same day the kernel moved on: `sf/kernel.bin` is `56759b9f…` since 22:43Z, this kernel plus one LinuxKPI fix for the GPU teardown panic; `sf/kernel.bin.old` is `821cb932…`. See SF-R-storage.md and SF-AC-vulkan.md.)
SiliconForest on the disk with `sf/kernel.bin` `821cb932…` (trackpad swipes), the standard state of the
night: `/usr/bin/chonkstep-darwin-desktop.next` = desk-next-011 `bb9a2c9b…` (with the inject module,
running), `/usr/bin/chonkstep-darwin-desktop` = desk-daily-011 `71da0543…` (without it),
`.safe` = desk-daily-001 `b1104b75…`, the dock running. Patches 6-11 in
`integrations/puredarwin/terminal-app/compositor/` give the source of 011 from `wm-darwin-cmdq-src`
(checked with `diff -r`). With desk-next-009 a swipe from a desk with one Space made Space 2 and
came back (constructed four-finger reports through the driver: `release position=1.162
velocity=3.655 target=1`, `WMSPACE current=2 count=2`, back `target=-1`).

## Incident
desk-002 ended at start on the M1 ("standalone keyboard contract not installed": the shell refuses the
Spaces profile on a backend that does not admit it). sf-session then restarted the machine; it came
back into Linux; the next SiliconForest boots ended the same way before Wi-Fi was up, so SSH never
reached them. My recovery polling also tripped the Linux SSH rate limit twice. Fixed by one bench
install boot (dr60: RAM root, `DR_RW=1 DR_HALT=reboot DR_INSTALL=1 DR_COMPOSITOR=desk-daily-001`):
`NV_SFINSTALL rc=0`, `SF_HALT stop … stopped-clean`, `DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`,
reset_attempts 0. About an hour of M1 time lost for the other agents (09:24Z–10:22Z). Since then
every build is host-tested first and the session falls back to `.safe`.

## 7. Not done (in the order a Linux user would miss them)
Recording and the capture toolbar; the Overview morph on vertical swipes; the Alt-Tab switcher was not
checked on this desk; snapping keys (Super-arrows move/focus are bound, not checked); notifications
(Omarchy's shell does not exist here: only the desk's one-line notices); lock screen (none);
brightness/volume OSD beyond the one-line notice; the Clip's Space number; dockapps; the Linux
dock's instrument data on Darwin; window restore position after maximise (TERM saw (0,0)).

## Taken from Finch
Nothing.

## macOS key map and recording (DESK2, 14 November, evening)
The owner: "I want the muscle memory to be macOS. All feels native to Mac OS please and btw do we
support screen recording as well", then: application icons on every launcher, the dock's right-hand
column as one themed SiliconForest set with real figures, maximise honouring it.

**Truthfulness.** "Observed" below = seen on the M1's disk system with **synthetic input** (the
compositor's inject module in the test build `.next`, or the same-uid capture socket from SSH), judged
from the compositor's own captures (looked at) and its log. Nobody looked at the panel, no real key
was pressed and no real finger touched the trackpad. Captures: `runs/m1-desktop-010/desk2-tests/shots/`
(not committed), recordings in `desk2-tests/rec/`.

### What a person at the M1 can now do
- macOS's own chords (table below): Command-Tab switcher with the applications' icons, Command-Space
  launcher, Command-Shift-3/4/5 screenshots and the capture toolbar, screen recording, Command-H and
  Command-Tab to get it back, Force Quit, Control-arrows/-digits for Spaces. A Mac application keeps
  every Command chord the system does not take (Command-T, -F, -S, -O, -<digit>, -Q, -W, -M, Control-Command-F ...).
- Record the screen or a selection (toolbar, `--screencapture -v`), stop it with Command-Control-Escape
  or the indicator at the top right; files "Screen Recording <date> at <time>.mov".
- The dock: real icons (Terminal and Activity Monitor from their bundles, our drawn terminal icon),
  a round running dot, tiles that launch the right application (or bring it forward when it runs,
  hidden or not), and on the right the SiliconForest column: the mark, CPU/MEM with figures
  ("5%  1.1G"), NET, the clock (digital, a click switches to analog), all in the desktop's theme.

### The key table (Command = the Mac keyboard's Command key)
macOS wins every clash. The Linux desk's verbs that macOS lacks moved to chords macOS leaves free, by
one rule: Spaces verbs keep macOS's Control (Control-Shift-<n> takes the window to Space n,
Control-Option-Shift-<n> sends it, Control-Shift-Left/Right take it along), window verbs are
Control-Option (as Rectangle and other macOS window managers use it). The full list:
`cargo test -p chonk-shell standalone_key_table_dump -- --ignored --nocapture`.

| macOS chord | Action | State |
|---|---|---|
| Command-Tab / Command-Shift-Tab | application switcher: per application, icons and name, held Command; Tab/Shift-Tab/arrows/` step, Q quits the selected application, Escape cancels, releasing Command switches | **observed** (`d2-k-switcher`, `d2-f3-switcher`, commit; Q quit Terminal.app: it got its own Command-Q); Shift-Tab bound, not exercised |
| Command-` / Command-Shift-` | the front application's next/previous window | bound, not exercised |
| Command-Space | launcher (Spotlight's place): applications in /Applications, /System/Applications (and Utilities) and the desk's terminal; type, Up/Down, Return opens or brings forward, Escape clears/closes | **observed** (`d2-h-launcher`: "act" -> Activity Monitor opened; "terminal" -> Terminal.app opened and got the focus) |
| Command-Shift-3 | screen to a file | **observed** (135,829 bytes, 228 ms) |
| Command-Shift-4 | drag an area; Space: pick a window (the window alone with its frame); Escape / right click cancel | **observed** with Space: window 1327x890 in 37 ms; the drag itself not exercised this round |
| Command-Shift-5 | capture toolbar: Entire Screen, Window, Selection, Record Screen, Record Selection, Options (Save to Desktop/Documents/Pictures/Screenshots/Clipboard, Timer None/5/10 s, Show Mouse Pointer), Capture/Record, × | **observed** (`d2-e-toolbar`; Record Screen and Record Selection from it); the Options choices bound, not exercised |
| Command-Control-Shift-3 / -4 | the same to the clipboard (`image/png`) | bound, not exercised |
| Command-Control-Escape | stop a recording | **observed** |
| Command-Option-Escape | Force Quit Applications (Return, Return again = SIGKILL) | list **observed** (`d2-f7-forcequit`); the kill not exercised |
| Command-H / Command-Option-H | hide the application / the others (the desk hides the windows; Command-Tab or the dock tile shows them) | Command-H **observed** for the desk's terminals and for Terminal.app, back by Command-Tab (`d2-f2-hidden`, `d2-f4-back`); Option-H not exercised. AppKit's own Hide did nothing visible for Terminal.app (Command-H reached it with build 006), hence the desk's |
| Command-Q, -W, -M, -Option-M, Control-Command-F, Command-N | the application's (its menu's): a Mac application gets them; on the desk's windows: quit, close, minimise, minimise all, full screen, new terminal | Control-Command-F **observed** with Terminal.app (full screen on Space 2, back at 865x479); Command-Q to a Mac app observed (via the switcher's Q); native ones host-tested, not exercised on the M1 |
| Control-Left / Control-Right | previous / next Space | bound; exercised with one Space only (nothing to switch to, as on macOS) |
| Control-Up / F3 | Mission Control (Overview) | bound, not exercised this round (whether the Touch Bar's F3 reaches us: unknown) |
| Control-Down | the front application's windows (App Exposé) | bound, not exercised |
| Control-1 ... Control-0 | Space 1 ... 10 | bound, not exercised |
| Command-F3 | show the desktop | bound, not exercised |
| Control-Command-Q | lock screen | **not available**: there is no lock screen; unbound (goes to the application) |
| Shift-Command-Q | log out | not bound (no log-out dialog) |
| three/four fingers left/right, up, down | next/previous Space (content follows the fingers), Mission Control, App Exposé (down, new) | swipe code of DESK unchanged; down -> App Exposé not exercised; real fingers untested |
| Control-Command + two-finger scroll | the swipe stand-in (kept) | not exercised |
| Control-Shift-<n>, Control-Option-Shift-<n>, Control-Shift-Left/Right | take / send the window to a Space | bound, not exercised |
| Control-Option-Return / -F / -arrows / -Shift-arrows / -=/- / -T / -L / -Shift-L / -S / -Shift-S / -Tab / -Escape / -Space | maximise / tiled full screen / focus / move / size / float / layout / freeform / scratchpad / to scratchpad / last Space / window menu / desktop menu | maximise **observed** (`d2-z4c-max`: the frame ends at the column); the others bound, not exercised |

### Screenshots, the toolbar and recording
- Files: "Screenshot <date> at <time>.png", "Screen Recording <date> at <time>.mov", mode 0600, never
  replacing a file, in `~/Pictures/Screenshots` by default: the session's HOME (/var/root) has no Desktop
  folder and nothing here shows one (no Finder), so the macOS default was not adopted; the toolbar's
  Options offer Desktop and Documents, kept in `/private/var/db/desktop-state/screencapture`. No
  floating thumbnail.
- Command line: `chonkstep-darwin-desktop --screencapture [-c] [-w | -R x,y,w,h] [file.png]`, and
  `--screencapture -v | -V <seconds> [-F] [-R x,y,w,h] [file.mov]` (Return or Control-C stops `-v`).
- Recording: no hardware encoder and no ffmpeg on the M1 (not installed: checked), so each frame is
  scaled by the GPU (`glBlitFramebuffer`, then a small `glReadPixels`) and compressed to JPEG by the
  CPU on three worker threads into a QuickTime movie (Photo-JPEG; ffprobe: mjpeg, decodes with ffmpeg).
  Frames are taken only when the screen changed, at most 30 a second; default size half the panel's
  pixels (1280x800), `-F` the full panel. **Measured on the M1:** 1280x800: read 25 ms on the
  compositor's thread + JPEG 43 ms per frame on a worker; 35 frames in 6.0 s (5.8 fps, as often as the
  screen changed) at 50 % of one core; a selection 500x300: read 5 ms, JPEG 8 ms, 15 % of one core;
  full 2560x1600: read 99 ms, JPEG 174 ms, 3.7 fps, 108 % of one core. The first attempt read the
  whole panel (100 ms per frame on the compositor's thread, 2 fps): replaced. At high frame rates the
  25 ms read costs the desk responsiveness (not measured). **No sound** (not trivial: no audio capture
  path here). The indicator ("● 0:07 Stop", top right of the work area) is in the recording, as the
  pointer is.
- Privacy as for screenshots: only the compositor's keys/toolbar or a same-uid socket connection start
  a capture or a recording; the installed daily build has no inject module (`WMINJECT absent`, no
  `armed`), nothing reads a request from a file.

### Icons
`wm-theme/src/app_icon.rs`, used by the dock (launcher and window tiles), the switcher, the launcher,
minimised-window tiles (a badge) and Overview cards (a badge). A bundle's icon is decoded at run time
from the bundle in place (CFBundleIconFile -> .icns; PNG, ARGB and RLE+mask entries; JPEG 2000
skipped), kept only in the process's memory; an application id maps to its bundle by
CFBundleIdentifier, else by its process's executable (`proc_pidpath`) -> enclosing .app. **Decoded on
the M1:** Terminal.app (Terminal.icns) and Activity Monitor (AppIcon.icns), both PNG entries; the RLE
and ARGB decoders are host-tested only (no bundle here uses them); no bundle needed its Assets.car.
The desk's terminal uses our own drawn icon (`Icon=chonkstep-terminal`); an application without any
icon gets a drawn generic one; the two-letter tile stays only where nothing else exists.

### The dock (SiliconForest)
- Items: `integrations/puredarwin/desktop/system/applications/*.desktop` in `/usr/share/applications`
  (`Icon=` a bundle path, a file, a hicolor name or `chonkstep-terminal`); pins in
  `$XDG_STATE_HOME/chonkstep/dock` (one desktop-file id a line). The Mac applications' tiles run
  `chonkstep-darwin-desktop --desktop-request application <bundle id>`: brought forward when running
  (also when hidden or windowless: AppKit's `org.puredarwin.appkit.reopen`), else opened, its first
  window then gets the focus.
- The column (Darwin build only; `dock/crates/chonk-dock/src/siliconforest.rs`): the mark, CPU/MEM,
  NET, clock; no sound/link/Bluetooth/power tile, no Clip. Real data from Darwin's counters
  (`widgets/darwin_proc.rs`: host_processor_info, host_statistics64 + hw.memsize, getifaddrs);
  en0's receive counters are 0 in the kernel today, so NET shows what the driver counts. Everything
  takes the active theme's tile material, glass, LED ink and fonts, and the dock follows the desktop's
  theme (`$XDG_STATE_HOME/chonkstep/theme`, re-read every 2 s): observed in Relay and Amber Phosphor
  (`d2-z5-amber`, `d2-z6-relay`), previews of four themes at 56/84/112 px from
  `DOCK_PREVIEW_DIR=<dir> cargo test -p chonk-dock column_preview -- --ignored`.
- Clock style: a click on the clock switches digital <-> analog, kept in
  `$XDG_STATE_HOME/chonkstep/dock-clock` (`digital` or `analog`; observed).
- Geometry: the column is as tall as its tiles (observed 112x448 at the top right); the compositor
  reserves its width over the full height for maximise/tiling (work area 2448 wide, observed), while the
  area under it is ordinary desk: no surface there, a window dragged into it stays (observed,
  `d2-z4b-dropped`). Full screen covers everything (observed).
- The mark is a first draft for the owner to judge (vector source:
  `integrations/puredarwin/desktop/branding/siliconforest-mark.svg`, the same geometry in code).
- Before/after: `runs/m1-desktop-010/desk-tests/shots/t7-a-dock.png` (the night's dock: the Chonkstep logo, NET/SND/LNK/BT/PWR showing dead screens, CPU/MEM, the analog clock, the column's band down to the Clip at the bottom) / `desk2-tests/shots/d2-final-daily.png` (made by the daily build through the socket).

### Faults
- **Dock tiles did not launch** (Activity Monitor, Terminal.app): at the M1's 150 % the compositor gave
  layer surfaces pointer coordinates in fractional units while the dock's surfaces use the whole buffer
  scale (2): a click landed a tile further down (often past the last one). Fixed in the compositor
  (input.rs, layers.rs, popups.rs). Observed: the click on the Terminal.app tile made the dock spawn
  its Exec (dock log with RUST_LOG); the Exec's request opened Activity Monitor with the daily build.
- **Exit Full Screen ended at the work-area size** (TERM 4.1/5.2): the client acknowledged the
  old-size configure but committed one more full-screen buffer, which was adopted (cut to the work
  area) and configured back. A buffer larger than the size acknowledged within a second of a
  full-screen/maximise change is no longer adopted. Host test `a_stale_full_screen_buffer_after_exit_
  is_not_adopted` (fails without the change); observed with Terminal.app: `WMFIT ... stale buffer
  1707x1067 ... (asked 865x479): not adopted`, the window back at its old size.
- **Stale minimised tiles** (TERM 5.1): not reproduced (host test with a client that goes while
  minimised passes; not seen on the M1 this round). Open.
- **"S..." notices** and elided labels: the label drew with one face and elided by an estimate of
  0.75 em a character; it now measures. Observed: notices and launcher rows whole.
- **Window capture size**: a window capture is now the window alone with its frame (what covers it is
  not in it), the frame's rectangle (observed 1327x890).
- **New: a desk terminal ended ("duplicate key press")** after the switcher: Command's release was
  taken while its press had gone to the client, so smithay listed Command as held at the next focus.
  Fixed; host test `after_the_switcher_no_key_is_left_held_for_the_client` (fails without it);
  observed: both desk terminals alive after Command-Tab then Command-Space.

### Not done
The toolbar is text buttons (no symbols), no floating thumbnail, no adjustable selection; the
switcher's H and mouse; Command-Tab-Q closes the switcher (macOS keeps it open); the launcher finds
applications only; lock screen and log out (none); dock hover tooltips (labels kept), a slimmer
column (tiles stay 56 pt); a running dot for an AppKit application without windows (the dock knows
only toplevels); AppKit's own Hide (menu) path (request to CLAUDE); recording audio, pointer
on/off for recordings, frame rates above what the screen changes at (not demonstrated); every chord
marked "bound, not exercised"; real keys and fingers.

### On the disk and how to go back
- `/usr/bin/chonkstep-darwin-desktop` = desk2-daily-008 `3e89453a…` (no inject; running, no
  `/private/var/db/desktop-inject`), before: `.before-desk2` `71da0543…`; `.next` = desk2-next-008
  `da2be149…`, before: `.next.before-desk2` `bb9a2c9b…` (and `.before-desk2b..e`, test builds of the
  evening); `/usr/bin/chonkstep-dock` = desk2-dock-008 `53601217…`, before: `.before-desk2`
  `5f1baa06…`; `.safe` unchanged (`b1104b75…`). `/usr/share/applications/*.desktop` (3) replaced, the
  ones of before in `/private/var/db/desk2-keep/applications.before-desk2/`. New state file
  `/private/var/db/desktop-state/chonkstep/dock-clock`.
- Revert: `ln -f <path>.before-desk2 <path>.back && mv -f <path>.back <path>` for the three binaries,
  copy the three .desktop files back, `rm /private/var/db/desktop-state/chonkstep/dock-clock`, then
  `echo relaunch > /private/var/run/gpu-desktop/session-end-request; pkill -TERM -x chonkstep-darwin-desktop`.
- Source: compositor patch 12 `integrations/puredarwin/terminal-app/compositor/wm-darwin-macos-desk.patch`
  (gives the build sources of desk2-next/daily/dock-008 from `desk-src` exactly, `diff -r`).

### For the owner, at the M1 (real keys and fingers)
1. Command-Tab, keep Command held, Tab again, let go: the application you picked comes forward.
   With Command held, Q quits the selected application.
2. Command-Space, type "act", Return: Activity Monitor opens in front. Command-Space again closes.
3. Command-Shift-3: a notice "Screenshot saved". Command-Shift-4, drag: an area; Command-Shift-4, Space,
   click a window: that window.
4. Command-Shift-5, click "Record Screen", "Record": the indicator counts at the top right;
   Command-Control-Escape stops; the movie is in ~/Pictures/Screenshots.
5. In Terminal.app: Command-H hides it; Command-Tab brings it back. Control-Command-F and again:
   full screen and back at the same size.
6. Control-Right / Control-Left with two Spaces; three fingers left/right on the trackpad.
7. Click the dock's Terminal and Activity Monitor tiles (opened, or brought forward); click the clock
   (digital <-> analog); maximise a window (Control-Option-Return): it ends at the right column.

## Owner's first hands-on round (DESK3, 14 November, night)
The owner used the desktop by hand for the first time and reported seven things (cursor first: "the
most frustrating"); the coordinator added the clock and, later, the thin red line around Terminal.app.

**Truthfulness.** "Observed" = seen on the M1's disk system with **synthetic input** (the inject module of the
test build `.next`, and `--desktop-request` over the same-uid socket from SSH), judged from the compositor's own
captures (looked at, zoomed) and its log. Nobody looked at the panel; no real key, trackpad or finger. The owner
re-tests by hand (checklist at the end). Captures: `runs/m1-desktop-010/desk3-tests/shots/` (not committed).

| # | Owner's finding | Cause found | Fix | State |
|---|---|---|---|---|
| 1 | the pointer vanishes over the terminal after some resizes | (a) a client that set the same cursor surface again without a new buffer got no pointer at all (the compositor had taken the buffer at the first import and then only moved the hotspot of "nothing"); (b) a client's hide (`set_cursor(NULL)`) lasted forever: **Terminal.app hides the pointer while one types** (observed: `WMCURSOR hidden by the client` after two keys) and nothing showed it again; (c) when a move/resize grab ended over a window, that window got no `enter` until the next movement | the cursor surface keeps its last image (shown again whenever it is set); a client's hide lasts until the pointer moves 6 px or enters another surface (macOS's hide-while-typing); entering a window from nothing starts with the arrow; a destroyed or empty cursor surface shows the arrow; at the end of a grab the window under the pointer is entered at once. `WMCURSOR` log lines | **observed** (synthetic): pointer drawn over a terminal before, during and after an interactive resize (`d3-b`, `d3-c`, `d3-d`); typed into Terminal.app: pointer hidden (`d3-e-typed`), moved 30 px: arrow back (`d3-f-moved`, log `the pointer moved after a client hid it`). Host test `a_cursor_surface_set_again_and_a_hidden_pointer_that_moves_stay_visible` and `a_window_under_the_pointer_is_entered_when_a_grab_ends` (both **fail on the old source**, pass now) |
| 2 | the old bottom border stays during a resize | the Darwin backend never called wm-core's `flush_decorations()`: chrome repainted during an interactive drag (deferred to "before the next frame") waited until the drag ended | `flush_decorations()` before every frame; and during a resize each frame is drawn as it was for the size the client's picture has (the scene keeps the last few frame layouts and draws the one matching the picture), so frame and content change together: no old edge, no gap, no content under the frame | **observed** (synthetic): mid-drag capture `d3-c-resize-mid` (the frame around the terminal's picture, nothing left at the old bottom). Host test `the_frame_follows_an_interactive_resize_and_matches_the_clients_picture` (**fails on the old source**: row 84 shows the old edge across the content) |
| 3 | a thin white/grey line on the terminal over Activity Monitor | not reproduced with a native terminal over Activity Monitor at 150 % (`d3-d`: the overlap is one flat colour). With **Terminal.app** over Activity Monitor its terminal area is not opaque: the inject `dump` of its buffer has alpha 242 (0.95) in 865,595 of its 916,725 pixels (native terminals: 255 everywhere), and Activity Monitor's rows, grid lines and title show through it as faint lines (`d3-a-start`; also in COCOA4's `runs/sf-cocoa4/shots/m2-overlap.png`, through Terminal.app's scroller track). The client's alpha, not the compositor's blending (the GL renderer repaints the whole frame every frame: no stale damage). The old frame edge left across a window during a resize (finding 2) is the other way a grey line appeared on a terminal; fixed. Separately, client popups (menus, tool tips) were drawn above *every* window | popups are now drawn with their own window (a window stacked above covers its menus and tool tips; the pointer goes to a popup only where its window is uncovered). Translucency: request to COCOA4 (make the background opaque unless the profile asks for it; the desk has no blur) | popup stacking host-tested (lib/wire suites; no dedicated test) and **not observed** on the M1 (no lingering popup reproduced). The owner's line: probable cause (Terminal.app translucency) **not confirmed** — re-test by hand |
| 3b | a thin red line around Terminal.app (coordinator) | in **Terminal.app's own buffer** (inject `dump`: columns 0 and 1274 below the menu strip and the last row are (189,28,37) over a (24,28,37) background — red channel only; the rounded corners carry red-only coverage values) | evidence and request to COCOA4 (`runs/m1-coord/requests-for-CLAUDE.md`); compositor unchanged (frames identical for Mac and native windows) | open (AppKit/Onyx2D) |
| 4 | a double click on a launcher opens twice | nothing remembered a launch in progress | compositor: a repeated open of the same bundle (or of the desk terminal) while its first window has not come is ignored for up to 10 s (dock tiles, the Command-Space launcher and now the Applications menu, which also brings a running application forward instead of opening it again); dock: the tile's running dot pulses while the application starts and a second click is not a launch | **observed** (synthetic socket requests): three Activity Monitor requests 43 ms and 424 ms apart -> `WMMAC open ...` once and `ignored: already opening` twice, one process; two desk-terminal requests -> `WMCONTROL terminal requested` and `WMMAC terminal request ignored: one is starting (212 ms ago)`. Dock double click: host test `a_tile_clicked_twice_while_its_application_starts_launches_once`; compositor: `a_launch_repeated_before_its_window_comes_is_ignored` |
| 5 | right-click on a window title: maximise, show/hide the Mac menu bar, the app's menus | the standalone desk's window menu offered only Close (and the release of the opening click, which lands on the new menu, closed it at once) | the window menu (right click or Control-click on the title bar): Maximize/Restore, Minimize, Enter/Exit Full Screen, Move to Space ▸ (Spaces + New Space), Close (`xdg_toplevel.close` of that window only); it stays open after a click. For a Mac application: "Hide/Show Menu Bar" and its main menu as submenus (key equivalents right-aligned, disabled rows dimmed, ✓ for checked), fetched when the menu opens from the application's socket `$XDG_RUNTIME_DIR/app-menu/<pid>` (protocol in `wm-darwin/src/appmenu.rs`, proposed to COCOA4); no socket = the window verbs only | **observed** (synthetic): the menu on Terminal.app's and a terminal's title (`d3-g`, `d3-h`), Maximize then Restore from it (`d3-i`, `d3-j`); `WMAPPMENU ... no menu socket (the window's own verbs only)` — the application side does not exist yet. Host tests: `a_right_click_on_the_title_opens_the_window_menu_and_maximize_works` (wire), `the_menu_is_fetched_and_an_item_invoked_over_the_applications_socket` (a fake application on a real socket) |
| 6 | closing a child window ends Activity Monitor | compositor side checked: the close button, Command-W on a desk window and the menu's Close send `xdg_toplevel.close` to that one window; nothing signals a process (now logged `WMCLOSE window=... close request to this window only`). Command-W and Command-1…5 of a focused Mac application go to it (since DESK2) | the log line | quitting on a child's close is AppKit's (COCOA4) |
| 7 | clock hard to read; "1 . 1 G", "SAT 1 4"; NET "0 K"; "Activity …" | the dock's labels asked for the theme's face (Space Grotesk for Relay), absent from SiliconForest (fonts: DejaVu Sans, Inter, Noto Emoji, Noto Sans Mono CJK, Source Code Pro, URW): figures and punctuation came from another face, widely spaced. The clock's seven-segment digits were narrow, thin and drawn over ghost 8s. en0's receive counter is 0 in the kernel (the Wi-Fi driver counts what it sends only) | the dock loads the faces wm-theme carries (IBM Plex Mono/Sans, Space Grotesk) into its font database; all LED readouts use solid chamfered segments of a legible shape; the clock shows lit segments only, as tall as the glass allows, in every theme; NET shows no figure (blank digits) for receiving on an interface whose receive counter never moved while its transmit counter did (en0 today: **receive is not available from the kernel counters**; transmit is shown); a long name on a launcher tile is set smaller (to 3/4) before it is cut | host previews (`DOCK_PREVIEW_DIR=… cargo test -p chonk-dock column_preview -- --ignored`, `-p chonk-dock-theme launcher_label_preview`) looked at in four themes; **observed on the M1** (desk3-dock-004, nextstep-classic, `d4-a-start` zoomed): "17%  0.5G", "EN0", "SAT 14" evenly set, the clock "21:53" in solid segments, NET's receive readout blank and transmit "1 K"; the launcher tile reads "Activity Monitor" in full. A hover tool tip was tried (desk3-dock-004) and removed: the dock's popups are parented to its right-hand column, so the tip was slid to x=0 over the tile and its dismiss backdrop took the pointer, the tip flickering every 0.6 s (`WMPOPUP new at=0,32 ... gone`, capture `d4-b-tip`); not in the installed dock |

Also (DESK2's list): Command-Tab switcher: the pointer over an application selects it, a click switches to
it, H hides the selected application and the switcher stays (wm-core `hide_application_of`, host test
`hiding_an_application_that_is_not_focused_keeps_the_focus`); **observed** (synthetic): the pointer over Terminal.app's icon selected it (`WMMAC switcher selected=0 app=com.apple.Terminal`), H hid its window with the switcher still open (`d4-d`, `d4-e`), releasing Command switched to it. Not done: a running dot
for a Mac application without windows (the dock knows toplevels only), stale minimised tiles.

### For the owner, at the M1 (real keys and fingers; one line per finding)
1. Pointer: type in Terminal.app (the pointer hides), move the trackpad: the pointer is back; resize a terminal by
   its corner and move into it: the pointer stays visible over it, before, during and after.
2. Resize a window by its bottom edge or corner and hold: the frame's edges follow the window while you drag; no
   bottom border left behind.
3. Put a terminal (Chonkstep's, then Terminal.app) over Activity Monitor: a thin line still showing through
   Terminal.app is its own translucency (COCOA4); one on the Chonkstep terminal would be a compositor fault: tell me.
4. Double-click the dock's Activity Monitor tile (or Command-Space "act" Return twice) while it is closed: one
   Activity Monitor; the tile's dot pulses until its window comes.
5. Right-click or Control-click a window's title: Maximize/Restore, Minimize, Enter Full Screen, Move to Space,
   Close; the Mac menu bar row and the application's menus appear once AppKit answers (not yet).
6. Close a second Activity Monitor window (its title-bar X): the application must stay (the compositor sends the
   close to that window only; if it quits, that is AppKit's). The clock column: the time, "SAT 14" and "17%  0.5G" read
   clearly; NET shows the send rate and no receive figure; the Activity Monitor tile reads its whole name.

### The theme and the dock's tiles (coordinator, after the first final install)
The desk had been left by DESK2 in Relay (wallpaper `relay`) with three launcher tiles (Chonkstep terminal,
Terminal.app, Activity Monitor). On the M1 `/private/var/db/desktop-state/chonkstep/dock` was rewritten at 20:29Z with
Activity Monitor only, and `theme`/`wallpaper` at 20:32Z with `nextstep-classic`/`lavender-grid` (a pick of the first
row of the desk menu's Theme submenu writes exactly these two). Both are before DESK3's first use of the M1 (21:31Z),
in the owner's own session; the session logs of then were rotated away by the reboots of 20:31Z and 21:06Z.
- Theme and wallpaper: most likely the owner's own choice; **left as they are** (the coordinator asks him).
- The pins: one way the dock loses tiles by itself was found and fixed — a click whose pointer drifted a few pixels past
  the strip's edge before the release (a trackpad click at the edge of the last tile) was taken as a drag off the strip
  and unpinned the tile (host test `a_click_that_drifts_off_the_strips_edge_does_not_unpin`, **fails on the old dock**:
  the bottom tile is unpinned by a 6-pixel drift). A tile is now unpinned only when dropped at least half a tile away
  from the strip, and a drop where it was writes nothing. The three pins of `integrations/puredarwin/desktop/system/
  dock-pins` were **restored** (the file of 20:29 kept as `dock.before-desk3-restore`).
- The title bar of the window at the panel's left edge (`d5-final-daily`): light grey with a close box only =
  NeXTSTEP Classic's inactive title bar (that theme has miniaturize at the left and close at the right, no zoom); its
  miniaturize button lay under the launcher strip. The owner wants maximise to honour only the right column ("we are ok
  with it overlapping launchers and minimized tiles"), so the strip still reserves nothing; instead a window placed for
  the first time is moved clear of the strip (`place_clear_of_launchers`, log `WMLAYER placed window=… clear of the
  launcher strip`); maximise and dragging may still cover it. Not exercised on the M1: the terminal requested after
  the relaunch was placed at the lower left, clear of the strip already; the session's startup terminal still lies
  under the strip, because it maps before the dock starts (`d6-restored-daily`: the three tiles back, NeXTSTEP
  Classic kept).

### Host tests (final source, `runs/m1-desktop-010/desk3-tests/`)
wm-darwin (`--features desktop-shell`): lib 137/137 (with the launch-debounce and app-menu tests); wayland_shm 16/17,
wayland_scale 3/4, wayland_dmabuf 5/5, wayland_theme 3/3. The two failures are the base's
(`autopilot_exercises_the_whole_desktop_against_a_real_window`, `a_window_too_large_for_the_panel_at_a_scale_is_
shrunk_and_gets_its_size_back`); the autopilot's failure point varies from run to run (its idle phase overflowed in
three of three runs of the final source on an idle machine, and in one of two runs of the source without the
switcher change, which reached its settings picks in the other; once, under a load of three builds, the final
source's settings picks failed): timing-dependent, not caused by this round as far as these runs show.
New tests that fail on the old source (`desk3-basecheck`): the pointer, the grab-end enter, the resize frame, the
window menu. chonk-shell 303/303 (+4 ignored), wm-core's new switcher-hide test, chonk-dock 206, chonk-dock-theme
110, chonk-instruments 254 (+1). `python3 tools/check.py --sanitize --cross`: rc 0.

### On the disk and how to go back
- `/usr/bin/chonkstep-darwin-desktop` = desk3-daily-008 `43f174ea…` (no inject; running; `/private/var/db/desktop-inject`
  and the runtime inject directory removed), before: `.before-desk3` (desk2-daily-008 `3e89453a…`); `.next` =
  desk3-next-008 `95202f4d…` (inject), before: `.next.before-desk3` (desk2-next-008); `/usr/bin/chonkstep-dock` =
  desk3-dock-008 `e196d285…`, before: `.before-desk3` (desk2-dock-008 `53601217…`); `.safe` unchanged (`b1104b75…`).
  `/private/var/db/desktop-state/chonkstep/dock` restored to the three default pins, before: `dock.before-desk3-restore`
  (theme and wallpaper untouched).
  Hashes taken right after each copy, before anything ran them (a running binary reads back with its PAC
  instructions lowered to NOPs: the daily then hashes differently, not a change on the disk).
- Revert: `ln -f <path>.before-desk3 <path>.back && mv -f <path>.back <path>` for the three binaries (and
  `mv dock.before-desk3-restore dock` in the state directory if wanted), then
  `echo relaunch > /private/var/run/gpu-desktop/session-end-request; pkill -TERM -x chonkstep-darwin-desktop`.
- Source: compositor patch 13 `integrations/puredarwin/terminal-app/compositor/wm-darwin-owner-round.patch` (applied
  to a fresh copy of `desk2-src` it gives the build sources of desk3-next-008, desk3-daily-008 and desk3-dock-008
  exactly, `diff -r`).
- Captures (`runs/m1-desktop-010/desk3-tests/shots/`): `d3-a-start` (Terminal.app over Activity Monitor), `d3-b`/`d3-c`/
  `d3-d` (pointer and frame before/during/after a resize), `d3-e-typed`/`d3-f-moved` (hidden while typing, back on
  moving), `d3-g`/`d3-h` (window menus), `d3-i`/`d3-j` (Maximize/Restore from it), `d4-a-start` (the dock column and
  strip), `d4-c`..`d4-f` (switcher: pointer, H), `d5-final-daily` (the daily build, NeXTSTEP theme and one tile),
  `d6-restored-daily` (the daily build after the pins' restore, through the socket), window dumps
  `d3a-*`, `d4a.txt`.

### Protocol for the application's menu (compositor side done, AppKit side asked of COCOA4)
`$XDG_RUNTIME_DIR/app-menu/<pid>` (the session's runtime directory, `/private/var/run/gpu-desktop`): a Unix socket
the application listens on, owned by the session's user. The compositor connects only when the title-bar menu of a
window of that pid opens (checks: socket owner = its uid, peer pid = the window's client pid; 200 ms timeout), sends
`menubar` (answer `shown`/`hidden`) and `menu 1` (answer: `item <id> <depth> <flags> <key> <title>` lines, then
`end`; flags `s` submenu, `d` disabled, `c` checked, `m` mixed, `p` separator, `a` the application menu; key
`cmd+shift+opt+ctrl+<char>` or `-`), and on a pick `invoke <id>` or `menubar toggle` (answer `ok …`/`error …`).
Nothing is sent at any other time. COCOA4 had proposed a private Wayland protocol (`pd_app_menu_v1`) at the same
time; the answer in `runs/m1-coord/answers-from-DESK3.md` asks for this socket instead (already exercised here).

### Taken from Finch (DESK3)
Nothing.

## Stability and multi-output (DESK4, 15 November)

**Truthfulness.** All input below was synthetic: keyboard records through the real HID queue from the
Touch Bar driver's test entry (`kern.pd_touchbar.inject`, F1 presses; the brightness was put back), a
competing reader on `/dev/usb_hid_kbd`, and pointer motion from the compositor's inject module (which
since this round enters the same input-thread queue the device records enter). Nobody touched the M1.
The owner's hands are the only proof of the real keyboard/trackpad path: see the checklist at the end.

### 1. The 00:53Z drop: real input ended the desktop
Evidence: GFX2's UART capture (`runs/sfvk/gfx2-present-race-001-uart.txt`; the session logs of that boot
were rotated away by the later reboots). The compositor had run 2059 s; from 00:52:37Z the owner opened
menus and closed windows at the M1; at 00:53:10Z the frame loop ended with `PDHID sequence
discontinuity: queue overflow or competing reader; input state is lost`, the session ended and
`SF_HALT` restarted the machine into Linux.
- **Cause, compositor.** `hid.rs` treated every irregularity of the kernel's HID queues as fatal: a gap in
  the sequence numbers (the kernel keeps 64 records per queue and overwrites the oldest when the reader
  is late, `PDHIDEventQueue.cpp`; a second reader takes records too), a key usage without a key code
  (LANG1/2, International, Help, ... would have ended the desktop as well), a malformed or short record,
  any read error. The queues were read once per frame, in the frame loop. What made the reader late at
  00:53 is not established (no frame-time record of that second survives).
- **Fix (patch 14, `wm-darwin-input-robust.patch`).** Nothing the queues give can end the desktop: a gap
  releases the keys held (their releases may be among the lost records) and goes on; the pointer's next
  record carries all its buttons; bad records are dropped; a node that keeps failing (3 errors in a row,
  or EOF) is closed and reopened with back-off 50 ms .. 5 s (the keyboard's console grab follows the
  open file); each kind is said once (`WMHID ... (said once; counted from now on)`) and counted, with a
  count line at most once a minute. The queues are read on a thread of their own (`pdhid-reader`: every
  1 ms while input arrives, 4 ms after 250 ms quiet, 8 ms after 2 s), whatever the frames do; arriving
  input ends the wait between frames (a pipe in the frame loop's poll). An event the desktop cannot use
  is logged (`WMINPUT ... (not fatal)`), never the end of the session; the shell's motion/click queues
  drop their oldest entry instead of failing; a failing key binding is logged.
- **Host tests** (`hid.rs`): `real_input_faults_never_end_the_session` (gap with keys held, LANG1, a
  malformed and a short record, a 200-record pointer gap) **fails on the old source with the M1's own
  message** (`runs/m1-desktop-010/desk4-tests/basecheck-hid-old-source.txt`);
  `the_reader_thread_keeps_up_while_the_desktop_is_stuck_in_a_frame` (a 64-record ring fed at 1 kHz:
  read once per 300 ms frame it loses records, on the thread none of 600);
  `a_failing_node_is_reopened_with_back_off_and_the_wake_pipe_rings` (EIO x3, two failed reopens,
  50/100/200 ms).
- **Observed on the M1** (desk4-next-002, synthetic through the real queue,
  `runs/m1-desktop-010/desk4-tests/m1/`): 40 F1 presses (80 records) while the frame loop was stalled 2 s
  (inject `stall 2000`): all 80 read, 0 gaps, 80 key events delivered. 100 presses with a competing
  reader taking records off `/dev/usb_hid_kbd`: the reader took 5, the compositor logged `WMHID keyboard
  queue: 1 records lost ... held keys released, input goes on`, counted gaps=5 lost_records=5, and stayed
  up. **Not reproduced:** a real overflow of the 64-record ring. `kill -STOP` does not stop a process on
  SiliconForest (`ps` shows `S`, the compositor kept reading 200 records): a kernel finding, not chased.

### 2. A compositor exit restarted the machine
**Cause.** `sf-session` replaced a compositor by `.safe` only when it ended with an error *within 20 s of
its start*; this one had run 34 minutes, so the session ended, and the end of the session is a restart.
**Fix** (`m1-storage/system/sf-session`, function `decide_after_end`, host test
`desktop/tests/test_sf_session.sh`, 21 cases): only an explicit request (`restart`, `reboot`, `poweroff`,
`shutdown` in `session-end-request`; the desktop menu writes the first and third) ends the session. A
compositor that ends by itself (any exit code, a signal, an unknown request word) is started again: the
same build once, then the installed daily build, then `.safe`, from the third end in a row with a pause
growing 5 s per end (at most 60 s); a build that ran 10 minutes starts the count afresh. The new compositor
shows a notice (`CHONKSTEP_DARWIN_START_NOTICE`: "The desktop restarted after an error; its windows were
closed"), the session logs `SF_SESSION unexpected end N of <build> (rc, seconds, request): starting
<build>`, and the last 400 lines of the session log are kept in `/var/log/desktop-exits/` (the newest
20); `desktop-session.log` keeps three generations (`.old`, `.old2`, `.old3`). Bench boots
(`sfsession_seconds` or `sfbench`) keep the old rule. **The cost:** every window of the session ends
with the compositor (they are its Wayland clients); Terminal.app, Activity Monitor, a test's windows go
too. **Observed on the M1** (after one reboot to start the new session): `.next` ended with an error
(inject `fail`) -> `.next` again with the notice (capture `desk4-tests/shots/desk4-notice.png`) -> again ->
the daily build -> SIGTERM without a request -> `.safe` after 5 s -> `relaunch-next` -> `.next` with
inject; no reboot; three files in `/var/log/desktop-exits/`.

### 3. Input independent of rendering; pointer latency (and late latching, asked by the coordinator)
With patch 14 the device queues no longer depend on the frame loop (above). The pointer still reaches the
screen only with a frame, and a frame only with a page flip. Measured with inject `stream` (absolute
motions, one every 8 ms, 1200 per 10-second phase, from a thread into the input thread's queue, i.e. the path device records take
after they are read; the kernel queue -> reader step, at most 1 ms while input arrives, is not in it) to
the flip-complete event of the first frame showing the cursor there (scan-out after the flip not
included):

| phase (10 s, ~600 frames each; desk4-next-007, `latch off`/`on` in one run) | oldest motion per frame p50 / p90 / p99 / max (ms) | newest motion per frame p50 / p90 / p99 / max (ms) | frames that missed the blank they aimed at |
|---|---|---|---|
| idle, latching off (as before) | 29.3 / 32.7 / 33.3 / 33.5 | 20.5 / 23.6 / 25.4 / 29.9 | 0 of 579 |
| idle, latching on | **18.9** / 22.2 / 25.5 / 39.5 | **10.2** / 13.9 / 16.6 / 30.8 | 1 of 575 |
| vkcube --present_mode 1 (mailbox), off | 29.4 / 32.8 / 33.3 / 33.4 | 20.7 / 23.5 / 25.0 / 26.1 | 0 of 608 |
| vkcube mailbox, on | **20.0** / 23.7 / 28.6 / 37.3 | **11.3** / 15.6 / 19.3 / 28.7 | 2 of 605 |
| the desk's terminal flooding (`seq 1 9999999`), off | 29.0 / 32.3 / 33.3 / 33.3 | 20.3 / 23.7 / 24.9 / 26.5 | 0 of 598 |
| terminal flooding, on | **21.5** / 26.8 / 29.8 / 35.5 | **13.2** / 18.2 / 21.0 / 22.8 | 2 of 603 |

Without late latching (every build before desk4-005) a frame is rendered right after the flip before it,
so a motion waits for that flip and then a whole refresh: about two frames (29 ms) to the oldest motion
a frame shows, 20 ms to the newest, the same idle and under GPU load (the input thread made no
difference here: the pointer never waited for the queue, it waited for the frame clock). **Late latching**
(patch 16, `FramePacer`): a frame is started as late as the next blank allows (the longest of the last 60
render times, each with how late the busy loop started it, plus a margin of 3.5 ms that grows by 1.5 ms
when a frame shows a refresh after the blank it aimed at and shrinks back slowly): about one frame
(19-21 ms) to the oldest motion and 10-13 ms to the newest. **The cost:** 1-2 frames in 600 (0.2-0.3 %)
miss their blank and show one refresh later (the frame before stays up 33 ms): a small stutter
regression against 0 without latching (desk4-005 with a 2.5 ms margin and no lateness term missed about
1 %, and with the terminal flooding it skipped a quarter of the blanks; desk4-006/007 fixed both).
`CHONKSTEP_DARWIN_LATCH=0` in the session turns it off. Before/after runs: `desk4-tests/m1/install-00{5,6,7}.txt`.
Next step for "no visible lag" (not done): the cursor on the panel's overlay plane (`--outputs` lists
one, `possible_crtcs=0x1`): a pointer-only frame would then be a plane move with no render, so its
budget could be ~2 ms instead of ~7 ms; the DCP's handling of a second plane (alpha, positions at the
edges) is unexplored.

Tearing is not possible on this path (whole-buffer atomic page flips). The vkcube zero-copy check
(`WMDMABUF check ... differing_pixels=0`) was read from the same runs: commits 30 and 150, 250,000 pixels compared, `differing_pixels=0`, `result=client-pixels-are-on-the-frame`, with latching on (desk4-005/006/007).

### 4. External display (groundwork; the kernel's half is CODEX-TB's)
Contract for the kernel: `runs/m1-coord/requests-for-CODEX-TB.md` (how the panel is driven today; K1-K12:
dcpext as a second CRTC/plane/connector on the same card node, DP/HDMI connector with EDID modes and mm
size, hotplug through a `kern.pd_dcp.hotplug_seq` sysctl because nothing like udev exists, independent
per-CRTC flips with `crtc_id` in the events, a modeset of one CRTC that leaves the panel alone, no hang on
a vanished link, 4K linear buffers, mirroring done by the compositor). Compositor side (patch 15):
- `outputs.rs`: the layout policy (macOS's defaults): the external is primary (dock column, launcher strip,
  new windows), the panel to its left, tops aligned; per-output scale (the panel keeps the owner's
  `scale` file; an external gets `scale-<id>` or a default from its density: a 27" 4K 150 %, 32" 4K 125 %,
  24" 1080p 100 %; never the panel's file); the pointer crosses where outputs touch and stops at other
  edges; on unplug windows come to the panel with their centre at the same relative place (shrunk when
  larger), Spaces of the external join the panel's of the same number; mirroring (one picture at the
  primary's logical size). Host tests with a virtual second output (plug, same set again, unplug, a panel
  briefly listed disconnected, a mode change, crossing, re-homing, mirroring, a scale change).
- `outputs::Watch` reads `kern.pd_dcp.hotplug_seq` twice a second, re-reads the connectors when it moves and
  logs the decision (`WMOUTPUT hotplug seq=N changes=[...] layout: ...`); this kernel has no counter (said
  once). `kms.rs` lists and logs every connector (`WMOUTPUT connector ...`) and takes the eDP panel when
  several are connected (before: whichever came first). `chonkstep-darwin-desktop --outputs` lists
  connectors/CRTCs/planes read only (observed on the M1: 1 CRTC, eDP-1 2560x1600@60 286x179 mm, a primary
  and an overlay plane, no hotplug counter).
- **Not done:** the second scanout (per-CRTC state and flips in `kms.rs`), rendering per output, a
  wl_output per output, the scene's global space with per-output scale, layer-shell surfaces bound to the
  primary, window re-homing and per-display Spaces wired into wm-core, mirroring's rendering. No nested or
  headless run with two outputs was made.

### 5. Leftovers
- Done: the session's startup terminals no longer lie under the launcher strip: when the strip maps for
  the first time, the windows already there are moved clear of it (`WMLAYER placed window=1 clear of the
  launcher strip: x 0 -> 112`, capture `desk4-tests/shots/desk4-start.png`; before:
  `desk4-notice.png`).
- Not done: a running dot (and reopen/quit) for a Mac application with no windows; stale minimised tiles;
  flicker of the dock's hover tooltips.

### Host tests (`runs/m1-desktop-010/desk4-tests/`)
wm-darwin lib 148/148 (007 source; every installed build host-tested first: 002, 003, 005, 006, 007), wayland_dmabuf 5/5, wayland_theme 3/3, wayland_shm 16/17 and
wayland_scale 3/4 (the base's two failures, unchanged), dock_host 1/1 (003 source, host dock build).
`test_sf_session.sh` 21/21.

### On the disk and how to go back
- `/usr/bin/chonkstep-darwin-desktop.next` = desk4-next-007 `045466b1…` (inject), `/usr/bin/chonkstep-darwin-desktop`
  = desk4-daily-007 `4a4ce94e…` (hashes right after the copy); the builds of before: `.before-desk4` (DESK3's
  desk3-next/daily-008), `.before-desk4b` (desk4-002), `.before-desk4c` (003), `.before-desk4d` (005),
  `.before-desk4e` (006) for both names. `.safe` unchanged (`b1104b75…`).
  `/usr/libexec/sf-session` new (`5fdab6d2…`), before: `sf-session.before-desk4`. New on the disk:
  `/var/log/desktop-exits/`, `/private/var/db/desk4/` (test scripts; remove).
- Revert: `ln -f <path>.before-desk4 <path>.back && mv -f <path>.back <path>` for both compositor names and
  `/usr/libexec/sf-session`; then a reboot (the running session keeps the old script open) or, for the
  compositors only, `echo relaunch > /private/var/run/gpu-desktop/session-end-request; pkill -TERM -x
  chonkstep-darwin-desktop.next`.
- Source: compositor patches 14-16 in `integrations/puredarwin/terminal-app/compositor/` (applied to a fresh
  copy of `desk3-src`, 14 gives the build source of desk4-002, 14+15 of 003, 14+15+16 of 007, exactly, `diff -r`).

### For the owner, at the M1 (real keys and fingers)
1. Move the pointer in circles and type fast in a terminal while opening menus and closing windows for a
   minute: the desktop stays; `grep WMHID /var/log/desktop-session.log` shows any queue gap it survived.
2. Drag a window quickly across the screen with Terminal.app scrolling output: the pointer keeps up, no
   jumps.
3. (Only if you want to see the restart path) `pkill -TERM -x chonkstep-darwin-desktop.next` over SSH: the
   desktop comes back by itself within seconds with a notice; the machine does not restart.
4. Shift held, then press a key while the trackpad is in use: no key stays stuck after letting go.

### Taken from Finch (DESK4)
Nothing.

## The pointer on its own plane, a second output, the dock's leftovers (DESK5, 15 November)

**Truthfulness.** All input below is synthetic: pointer records from the compositor's inject module (a
thread pushing absolute records into the input thread's queue, as the device records arrive) and keys
through the same queue. Nobody touched the M1. Latencies are from a motion's read (or injection) to the
flip-complete event of the frame that shows it; the panel's scan-out after the flip is not in them. What
the panel shows of the pointer's plane is **not observed by anyone**: the compositor's captures blend the
pointer in software where the plane is placed; only the owner can see whether the DCP draws it right
(checklist, first line).

### 1. The pointer: one frame or less
**Change (patch 17, `wm-darwin-cursor-plane.patch`).** The panel's CRTC has an overlay plane (id 37,
ARGB8888 among its 11 formats). The pointer now lives there (`kms.rs OverlayPlane`, `glrender.rs
CursorBuffers`): three 128x128 ARGB8888 buffers drawn by GL with straight alpha (the DCP treats ARGB as
not premultiplied: linux-aurora `apple_plane_atomic_update`), and the plane is part of every commit
while it is on. A move of the pointer alone is a commit that only moves the plane: no scene render.
The DCP takes no plane that leaves the screen or is smaller than 32x32 (`apple_plane_atomic_check`), so
the plane always lies wholly on the screen and near an edge the image is drawn shifted inside its buffer
(`place_cursor`). Client cursor images (hotspot, the 1.5 scale), a client's hide (plane off), the hide
until the pointer moves 6 px, and the theme's arrow all go through the same `set_cursor`; an image larger
than 128 px, a recording (its frames must contain the pointer), `cursorplane off`, or a commit the
display refuses (not EBUSY; then for good, with `WMCURSOR plane refused`) put the pointer back into the
frames. During a window drag the plane shows the pointer where the frame drew the window (no slipping);
`capture` and the autopilot's read-back blend the pointer in where the plane has it. Off switch:
`CHONKSTEP_DARWIN_CURSOR_PLANE=0` in the session.
**Timing.** A frame aims at a blank; it is committed `margin` before it and, when the scene changed,
rendered `render budget` before that (`input_latency.rs FramePacer`). Staged frames wait for their commit
moment, so the pointer is placed by the newest input even in a rendered frame. The margin is learned:
the display needs a commit about 2.0 ms before the flip event's time (sweep below), plus how late the loop
woke for its last 30 commits, plus 0.2 ms. Also: the loop's wake-up no longer rounds up to the next whole
millisecond (it made commits up to 1 ms late), a commit due is made first thing after a wake (before
clients are served), the frame loop and the input thread run at QoS user-interactive (looked up at run
time), and when a client's buffer comes too late for the next blank the pointer still goes on that blank
alone (without this a terminal flooding the screen held the pointer to every other refresh: desk5-001,
oldest 31 ms). The input thread polls the HID queues every 1 ms while input arrives, 2 ms after 250 ms
quiet, 4 ms after 2 s (8 ms before): the first motion after a pause waits at most 4 ms (the nodes have no
select; asked of the kernel).

Measured on the M1 (desk5-next-003, `desk5-tests/m1/install-003.txt`; plane off = DESK4's late latching on
the same build; 10 s phases, a motion every 8 ms, ~575 frames each):

| phase | pointer | oldest motion per frame p50 / p90 / p99 / max (ms) | newest p50 / p90 / p99 / max (ms) | missed blanks |
|---|---|---|---|---|
| idle | drawn into frames | 19.0 / 22.0 / 24.3 / 32.7 | 10.0 / 13.3 / 16.1 / 23.3 | 1 of 576 |
| idle | **plane** | **15.5** / 18.8 / 21.5 / 35.9 | **6.9** / 10.2 / 12.8 / 22.9 | 2 of 577 |
| vkcube mailbox | drawn | 22.7 / 27.1 / 30.6 / 31.4 | 14.1 / 18.1 / 22.1 / 24.2 | 1 of 614 |
| vkcube mailbox | **plane** | **15.5** / 18.9 / 21.6 / 39.0 | **6.9** / 10.2 / 12.2 / 30.2 | 3 of 611 |
| terminal flooding | drawn | 25.3 / 30.8 / 36.1 / 46.0 | 17.0 / 22.0 / 25.0 / 30.6 | 5 of 590 |
| terminal flooding | **plane** | **17.4** / 29.4 / 35.4 / 45.9 | **7.7** / 11.3 / 16.3 / 26.8 | 5 of 532 |

The installed build (desk5-next-005, `install-005.txt`, the same phases) gave the same: plane - idle 15.5 / 18.6 /
19.5 / 35.6 and 6.8 / 10.0 / 10.9 / 26.4 (2 of 576 missed), vkcube 15.6 / 18.5 / 20.9 / 35.1 and 6.9 / 10.0 / 12.8 /
26.4 (3 of 611), terminal flooding 16.9 / 29.3 / 35.5 / 48.4 and 7.5 / 10.8 / 13.5 / 24.2 (5 of 535; 319 rendered
frames, the rest pointer-only); pointer drawn - 21.2 / 24.6 / 29.0 / 37.9 and 12.5 / 16.0 / 19.9 / 29.2 (1 of 578),
20.3 / 25.1 / 29.6 / 34.9 and 11.6 / 16.8 / 20.7 / 26.2 (1 of 613), 23.9 / 27.9 / 30.2 / 37.8 and 15.2 / 19.2 / 22.0 /
23.4 (2 of 596). DESK4's numbers with the pointer drawn and late latching: oldest 18.9-21.5 ms, newest 10.2-13.2 ms;
the drawn path (now only a fallback: recordings, `cursorplane off`, a refused plane) is 1-2 ms slower than that
because its margin now follows the loop's recent wake lateness too.
At 125 motions a second and 60 frames, the oldest motion a frame shows is on average 12.7 ms old when the
frame is committed, so ~15.5 ms is about one refresh: the floor for this measure is that plus the margin.
**Commit lead sweep** (fixed margins, pointer-only frames, desk5-001/002/003): 1.0 and 1.5 ms: every frame
a refresh late; 2.0 ms: 89-99 % late; 2.5 ms: 5-31 % late (the ones whose real lead fell under 2 ms);
3.0 ms: 1-2 %. **Missed blanks are not 0**: 2-5 in ~600 frames (0.3-0.9 %), as with the pointer drawn
(0.2-0.8 % in the same runs). Every late frame had a commit made 0-2 ms before its blank, i.e. the loop
woke 1-2.5 ms late (also at QoS user-interactive); a larger fixed margin would trade latency for them
(3.0 ms: 1-2 % late). Not done: a commit thread with a real-time policy, which would remove the loop
from the commit's timing.
**Window drag** (the window follows the pointer: every frame rendered; desk5-next-003): oldest 18.2 / 21.5 /
22.4 / 22.8, newest 9.7 / 12.8 / 13.8 / 14.8, no missed blank during the drag; pointer drawn: 18.5 / 21.8 /
22.5 / 22.9 and 9.9 / 13.2 / 14.7 / 17.2 (desk5-next-005: 18.6 / 21.8 / 23.3 / 26.2 and 9.8 / 13.2 / 15.2 / 15.9 with
the plane, 18.4 / 21.7 / 22.8 / 23.1 and 9.7 / 13.1 / 14.4 / 15.1 drawn). That is the render budget (~2-3 ms) plus the
margin plus the refresh: about one frame to the oldest motion, the newest under one frame. During a drag the plane
follows the frame's pointer, so window and pointer move together. **Interactive resize: not measured on the M1.**
The instrumentation is in (`WMLATENCY ... resize asked->client-buffer | client-buffer->flip`: the client's round
trip, then the compositor's part), but none of three synthetic attempts started a resize (a press at the frame's
bottom-right corner border, Option + right drag (the standalone desk has no drag modifier), and a press at the
centre of wm-core's south-east resize hitbox, which the inject `dump` now reports): each ran as pointer motion with
the button held. Not chased further; a person's corner drag is the test (checklist). The compositor-side cost of a
resize frame is the same render as a drag frame (~1.8 ms, below); the client's redraw is the client's.

**Frame time before/after (single output).** With the pointer drawn, every move is a rendered frame (600 moves,
`desk5-tests/m1/framet5.sh`): GL draw + glFinish of the panel, before the second-output code (desk5-base004 =
desk5-003 + the timing) p50 1.76-1.77 ms, p90 1.79-1.80, p99 1.88-1.93, max 2.00-2.22 (two runs); after
(desk5-next-005) p50 1.75-1.76, p90 1.79-1.80, p99 1.92-1.97, max 1.96-2.13. No difference beyond run-to-run noise.

### 2. A second output (the compositor's half; no kernel exposes one yet)
**Change (patch 18, `wm-darwin-outputs.patch`).** The scene is one space in the panel's density
(`outputs.rs Layout::scene_outputs`): the panel at its origin, pixel for pixel as before, an external
display beside it (the primary, macOS's default) at a size of its pixels times the panel's scale over its
own (a 27" 4K at 150 %, like the panel: pixel for pixel; at another scale it is resampled: per-output
client buffer scales are not done). `displays.rs` makes the desk follow a set of outputs at run time:
wm-core's monitors (work areas, placement of new windows), Spaces per display (wm-core
`reconcile_display_spaces`; without separate Spaces the departed output's windows are rescued onto the
remaining ones), a `wl_output` per output with its own mode, scale and position (`xdg_output` gives the
logical size), each window's surface entering the output its centre lies on, the layer surfaces (the dock's
column and launcher strip) on the primary, the pointer crossing where outputs touch and stopping below
the shorter one. Rendering: the GL renderer draws the panel from the scene's origin as before and, for each
extra head, the scene's rectangle of that output into its own buffers (`u_view` in the shader).
Scanout: `kms.rs ExtraHead` - a second CRTC on the same card node for a named connector, its own primary
plane and mode, a modeset touching only it, its own flips (events routed by `crtc_id`), switched off on
unplug (contract K1-K12; additions K5/K13 in `runs/m1-coord/requests-for-CODEX-TB.md`). The hotplug watch
applies a new layout only when the renderer drives every output of it; otherwise the desk stays on the
panel and says why. The single-output path: no extra head, the panel's view is the origin (one more
uniform per frame); frame time before/after below.
**Tested:** host - `tests/wayland_outputs.rs` (a real Wayland client: plug a virtual 4K beside the panel,
a wl_output appears, the external is the primary and the scene 6400x2160, the window moved onto it is told
it entered `DP-1`, the pointer crosses at the shared edge and stops at the external's left edge below the
panel, unplug: the window is back on the panel, the external's wl_output is removed and the surface is on
the panel's again; the same with the desk's own session, where wm-core keeps the external's Spaces for its
return), `glrender a_second_outputs_head_shows_its_rectangle_of_the_scene` (GL on the host: the head's
picture equals the CPU render's rectangle of that output, every pixel), outputs/displays unit tests.
**Headless two-output run on the workstation** (`tests/headless_outputs.rs`, ignored by default; run with
`HEADLESS_DIR`): the desk's session on a Wayland socket, two real `foot` terminals (Linux programs), a
virtual 4K plugged at run time, the second terminal dragged by its title bar across the panel's edge onto
it by pointer events through the desk's input path (frame at 3437,688 on DP-1, the pointer on the
external), then the external unplugged: the terminal back on the panel (879,725). Pictures per output in
the run directory (`c-both-small.png`: the panel with the wallpaper and terminal-one, the external with
terminal-two focused). foot needs a `wl_subcompositor` global, which the desk does not offer (subsurfaces
are not composited); the headless run offers it for foot alone (`offer_subcompositor_for_host_clients`),
the installed desk does not. weston-terminal was refused (xdg window geometry with shadows is not
implemented).
**Not done:** a run against a real second display (no kernel has one); the wallpaper on the external (the
background image is the panel's size; the external shows the theme's colour); per-output client buffer
scale; mirroring's rendering; the desk menu's Display entries for the external; freeing the GEM buffers
of an unplugged head (kept for a replug at the same mode).

### 3. Leftovers
- **A Mac application without windows** (AppKit applications outlive their last window): the compositor
  publishes a foreign-toplevel handle for each one (a client still connected whose toplevels gave an
  application id, with no mapped window, serving an application-menu socket), title = its name, no state.
  The dock lights its tile's dot from it (`WMFOREIGN windowless applications: [...]`); the dock's click
  activates the handle and the compositor reopens the application (`org.puredarwin.appkit.reopen`,
  `WMFOREIGN reopen windowless application pid=...`); the handle's close quits it through the
  application's own menu (the item with Command-Q, over `$XDG_RUNTIME_DIR/app-menu/<pid>`; SIGTERM only
  without a socket). The Command-Tab switcher already listed such applications and reopened them; its Q
  now quits a windowless one through its menu too (before: SIGTERM). **Observed on the M1** (desk5-005 with
dock-005, synthetic input, `desk5-tests/m1/install-005.txt`, captures `desk5-tests/shots/i3-*`): Terminal.app opened
from the socket request, Command-W closed its window, the process stayed, `WMFOREIGN windowless applications:
["com.apple.Terminal(37756)"]`, and the dock's Terminal.app tile kept its dot (`i3-a-windowless`, zoomed in
`i3-tile-zoom.png`); a click on the tile: `WMFOREIGN reopen windowless application pid=37756`, `WMMAC reopen ...
posted=true`, the window back and the list empty (`i3-c-reopened`); Command-Q then quit it (its client
disconnected). Quit from the dock was not exercised (the dock's tile has no Quit item; the handle's close is what a
dock menu would send); the switcher's Q on a windowless application was not exercised.
- **Stale minimised tiles** (TERM 5.1): not reproduced. A new host test closes a minimised window while
  its application keeps running (the AppKit case) and the tile goes, as it does when the client
  disconnects (DESK2's test). The desk's own tiles are at the bottom left; the dock's at the bottom of its
  side. If the owner sees one again: `inject dump` lists every desk surface with its owner.
- **Dock tooltips**: a launcher tile shows its application's name beside it after a 0.5 s rest
  (`launchdock.rs tick_tip`): a plain tile of the strip's own kind on the Top layer, not a popup, no
  pointer grab, never under the pointer while it is on the strip; leaving the tile, another tile or a press
  takes it away at once. DESK3's attempt (a popup parented to the right column, slid to x=0 under the
  pointer, its dismiss backdrop taking the pointer) flickered every 0.6 s. Host test
  `a_tiles_name_shows_beside_it_after_a_rest_and_goes_when_the_pointer_leaves`. **Observed on the M1:** dock-005 drew
the name cut to "T..." (an estimate by character count, `i3-b-tooltip`); dock-006 sizes the label to the measured
text: "Terminal" whole beside its tile after a 0.9 s rest, gone when the pointer moved away (`f-tooltip-zoom.png`).
Seen there too: the name overlaps the tile's right edge by ~20 px - the dock's positions are converted at its own
buffer scale (2) while the desk is at 150 %, a mismatch older than the name (not fixed). Flicker is not judged by
captures; the design has nothing that could cause it (no popup, no grab, nothing under the pointer).
- **SIGSTOP** (for the kernel owner, `runs/m1-coord/requests-for-CODEX-TB.md`): minimal programs stop (six
  thread layouts, including a thread at user-interactive QoS and a poll on the DRM card node); the
  compositor does not (STAT `R`, threads `S`, CPU time +0.1 s in 2.5 s while "stopped"); reproducer
  `runs/m1-desktop-010/desk5-tests/sigstop/stopthreads.c` and the compositor check `desk5-tests/m1/sigstop5.sh`.
  A second kernel note there: the HID queue nodes have no select.

### 4. Requests
No requests file was addressed to DESK5. For TERM6's menu tests (the example in my brief): `motions <n>`
delivers the next n event steps in one frame (a device faster than the refresh); `stream` (timed, from a
thread) was already there; noted in `runs/m1-coord/requests-for-TERM5.md` with the pointer-plane notes
(captures blend the pointer in; `cursorplane off`).

### Host tests (`runs/m1-desktop-010/desk5-tests/`)
wm-darwin (`--features desktop-shell`, final source): lib 155/155 (with the plane placement, straight alpha,
pointer-only commits, read-back blend, pacer, motion-latency, scene-output, pointer-on-outputs and GL second-head
tests), wayland_outputs 2/2 (new), wayland_dmabuf 5/5, wayland_theme 3/3, wayland_shm 17/18 and wayland_scale 3/4 (the
base's two failures, unchanged: `autopilot_exercises_the_whole_desktop_against_a_real_window`,
`a_window_too_large_for_the_panel_at_a_scale_is_shrunk_and_gets_its_size_back`), headless_outputs 1/1 (ignored by
default; run by hand with foot). chonk-dock 208/208 (+2 ignored). Every installed build was host-tested first.

### On the disk and how to go back
- `/usr/bin/chonkstep-darwin-desktop.next` = desk5-next-005 `6443913601f4…` (inject; running), `/usr/bin/chonkstep-darwin-desktop`
  = desk5-daily-005 `d6de681b6a01…` (no inject); `/usr/bin/chonkstep-dock` = desk5-dock-006 `3e5ca103ac8b…`. Hashes taken on
  the M1 right after each copy. `.safe` unchanged (`b1104b75…`). The files of before: `.before-desk5` (DESK4's 007 pair and
  DESK3's dock), `.before-desk5-002/-003/-005` (my 001-003 builds; `.next.before-desk5-005` is the timing build
  desk5-base004, `.next.before-desk5-base` desk5-next-003), `chonkstep-dock.before-desk5-006` (dock-005). The session
  script, the theme (obsidian since 03:16Z, wallpaper `host:siliconforest` since 03:19Z: not changed by DESK5), the scale
  and the pins are untouched. `/private/var/db/desk5` removed; `/private/var/db/desk4` (DESK4's) left.
- Revert: `ln -f <path>.before-desk5 <path>.back && mv -f <path>.back <path>` for both compositor names and the dock,
  then `echo relaunch-next > /private/var/run/gpu-desktop/session-end-request; pkill -TERM -x chonkstep-darwin-desktop.next`
  (desk_m1.sh `relaunch next`). To keep 005 and only drop the pointer's plane: `CHONKSTEP_DARWIN_CURSOR_PLANE=0` in the
  session's environment.
- Source: patches 17-19 in `integrations/puredarwin/terminal-app/compositor/` (README there).

### For the owner, at the M1 (real keys and fingers)
1. **First: is the pointer there and right?** Move it around, over windows, to every edge and corner, over the dock;
   type in Terminal.app (it hides), move (it is back). It should look exactly as before (arrow, I-beam over text), with
   no dark or light fringe and no box around it. If it is missing, boxed or wrong: tell me, and put
   `CHONKSTEP_DARWIN_CURSOR_PLANE=0` into the session (the pointer is then drawn as before).
2. Move the pointer fast in circles with Terminal.app scrolling output: it should keep up with the finger (about one
   frame behind, as before only under load).
3. Drag a window quickly: the window and the pointer stay together. Resize one by its corner: the frame follows.
4. Open Terminal.app from the dock, close its window with Command-W: its dock tile keeps the dot; click the tile: a
   window comes back. Command-Tab shows it while windowless; Q there quits it.
5. Rest the pointer on a dock tile for half a second: its name appears beside it, whole, and goes when you move off.

### Taken from Finch (DESK5)
Nothing.

## Minimising and the dock's launchers (DESK6, 15 November)

**Truthfulness.** All input below is synthetic (the compositor's inject module in `.next`: keys, pointer, captures),
judged from the compositor's window list (`inject list`), its log (`WMMINI`, `chonk-dock:` lines), `ps` and its
captures (looked at, zoomed). The owner's own hands-on findings are the reason for this round; whether the result
feels right to his hands is his to say (checklist at the end). Captures: `runs/m1-desktop-010/desk6-tests/shots/`.

### 1. "Minimizing windows closes or kills them ... two icons ... then it closes altogether"
**Causes found.**
- **Terminal.app crashes when it is minimised** (AppKit, not the desk): after its own Command-M the window is put
  away and the process dies at once (`WMCLIENT disconnected ... ConnectionClosed`; kernel log `Terminal[651] Corpse
  allowed 1 of 5`, `EXC_CORPSE_NOTIFY ... etype=10` = EXC_CRASH; 2 of 2 tries). TERM8 found it from the crash report:
  `-[NSImage drawInRect:]` without a current graphics context in Terminal's miniaturise notification handler, a
  regression of their patch 0133; fixed in their candidate c11 (not installed when this was written).
- **A minimised window whose application took its picture away was forgotten.** An AppKit window ordered out
  while miniaturised attaches a NULL buffer; the desk treated that as the window going away (wm-core `Unmapped` ->
  forget), so its tile went and the window looked closed while the application ran on. Now such a window stays
  minimised (`WMMINI window=N ... unmapped by its client while minimised: kept`); its next buffer shows it again,
  and a click on its tile restores it at its place. Host test `a_minimised_window_whose_client_unmaps_it_stays_
  minimised_and_comes_back_at_its_place` (wayland_shm), **fails on the old source** ("the desk still has the window",
  `desk6-tests/basecheck-minimised-unmap-old-source.txt`).
- **Two icons**: a minimise drew the desk's tile (the window's picture with a 40 % application badge in its corner)
  and the dock's own icon tile of the minimised window (from the foreign-toplevel `minimized` state), both at the
  bottom left; and the desk's tile sat at desktop level, under the windows: a window over the bottom left hid it and
  took the click meant for it (my first synthetic run restored nothing for that reason).
- The native Chonkstep terminal was not killed by minimising in any run here. Note: the owner's theme now is
  System 7 (dark gray), whose title bar has a close box on the left and a zoom box on the right and no minimise
  button.
**Changes.** One tile: the window's picture (a dma-buf window's picture is read from its buffer through a CPU
mapping when it is minimised: `WMMINI preview window=N dmabuf ...`), its title under it, no badge, above the windows
(as macOS keeps minimised windows in its Dock); the Darwin dock draws no tile of its own for minimised windows.
No animation. `WMMINI` lines for every step (minimised, restored, unmapped while minimised, destroyed while minimised).
**Observed on the M1** (desk6-next-004, Command-M then a click on the tile, 10 rounds each, `desk6-tests/m1/t2.txt`):
desk terminal 10/10 and Activity Monitor 10/10 (each round: lifecycle Miniaturized, one square tile, the process
alive, back Normal, focused, at the same place); Terminal.app: crash (above); Firefox: **not tested** (my 18:50Z
relaunch had disconnected CODEX-FX's Firefox; I left it alone). Captures `t2-am-cmdm-min` (one tile with the
picture, over the terminal at the bottom left), `t2-am-cmdm-back`, `t1-desk-cmdm-min`. App exits while minimised:
the tile goes (`destroyed by its client while minimised: its tile goes`, seen with Terminal.app's crash). Not
exercised on the M1: the title-bar button (the owner's theme has none), the title-bar menu's Minimize, AppKit's own
Window > Minimize, a quit while minimised (host-tested: `a_minimised_windows_tile_goes_with_its_client`).

### 2. "The dock launchers disappeared when I clicked on them and tried to move them"
**Cause.** A press on a launcher tile became a drag after ~7 px; during the drag the dock only darkened the tile in
place (nothing followed the pointer, so it looked as if nothing happened), and a release anywhere half a tile or
more away from the strip **removed the tile at once and rewrote the pins file** - no confirmation, no undo. Moving
the tiles this way emptied the file; an empty strip is hidden. Firefox's installer then found the file empty and
wrote Terminal / Activity Monitor / Firefox, so the Chonkstep terminal stayed missing. Second, quieter way: the dock
read its application items once at start, so a pin whose item came later (Firefox's) did not resolve, and the next
change of the strip wrote the file without it.
**Changes** (`dock/crates/chonk-dock/src/launchdock.rs`, `runtime.rs`, `wayland.rs`, `main.rs`):
- a tile picked up follows the pointer; the strip makes room where it will land; the drop writes the order;
- dropped away from the dock it goes back to its place (nothing written); it is removed only when held away until a
  "Remove from Dock" label shows above it (0.8 s), or from its context menu;
- secondary click on a tile: Open (Show when it runs), Options > ✓ Keep in Dock / Remove from Dock, Quit (running
  applications: `chonkstep-darwin-desktop --desktop-request quit <app id>`, which quits a Mac application through its
  own menu's Quit, as the Command-Tab switcher's Q does, and closes a desk application's windows);
- the drag threshold is 8/56 of a tile (was 4/56), and a click that wobbles less than that launches its tile wherever
  it is released;
- ids that resolve to no item are kept in the file; the items are scanned again when the application directories
  change (and on `reload`); writes go to `dock.next` and are renamed over the file;
- **defaults**: a pins file that is missing or holds no id when the dock starts gets the default set:
  `chonkstep/dock-pins` in the data directories (`/usr/share/chonkstep/dock-pins` on SiliconForest, from
  `integrations/puredarwin/desktop/system/dock-pins`), else on Darwin the built-in `org.siliconforest.terminal`,
  `com.apple.Terminal`, `com.apple.ActivityMonitor`, `org.mozilla.firefox`. `chonkstep-dock --defaults` writes the
  set (the file of before kept as `dock.before-defaults`) and tells the running dock; from the workstation
  `desk_m1.sh dock-defaults`. The owner's pins were restored to this set (file of before: `dock.before-desk6`).
- on SiliconForest the dock places its surfaces in panel pixels (the margins were converted with the whole buffer
  scale while the pointer comes in panel pixels / 2, so anything placed by a margin - the tile's name, the dragged
  tile, its label, the instrument panels - sat at 3/4 of the pointer's position; seen in `d2-c-away`).
**Host tests** (chonk-dock 214/214 + 2 ignored): `a_click_that_wobbles_or_drifts_off_the_strips_edge_launches_and_
never_unpins`, `dragging_a_tile_along_the_strip_reorders_it_live_and_the_order_persists`,
`a_tile_dropped_away_from_the_dock_snaps_back_and_nothing_is_written`, `a_tile_held_away_shows_remove_and_only_then_
a_release_removes_it`, `a_tiles_context_menu_has_open_options_and_quit_and_removes_it`, `a_missing_or_empty_pins_
file_gets_the_default_set`, `a_pin_that_resolves_to_nothing_survives_a_move_in_the_file`. (DESK3's own test asserted
"a deliberate drag well away unpins": the old behaviour.)
**Observed on the M1** (desk6-next-004 with dock-001, `desk6-tests/m1/d2.txt`): move down two places and back, both
written (`moved from place 1 to 3 (written)`); a quick drop far away: back, nothing written; held away: the label,
then removed; `chonkstep-dock --defaults`: the four back; the context menu (`d2-h-menu`: Open, Options ▸); a click
wobbling 5 px on Activity Monitor's tile: it opened (`WMMAC open ...`). Captures `d2-a-move-mid` (the strip making
room, the tile following the pointer), `d2-b-moved`, `d2-c-away` (the tile at 3/4 of the pointer: the unit fault
fixed in 005), `d2-e-remove-label`, `d2-f-removed`, `d2-g-defaults`.

### 3. Requests addressed to the desk
- **Pinch/rotate**: the compositor offers `zwp_pointer_gestures_v1` and gives pinches to the client under the pointer
  (begin with the fingers; updates with the centre's motion, the scale since the begin and the turn; end/cancel).
  Host test with a real client: `a_pinch_goes_to_the_client_under_the_pointer_through_pointer_gestures`. The trackpad
  driver does not report pinches (`M1TP_MT_G_IGNORED`): the kernel requirement is **K14** in
  `runs/m1-coord/requests-for-CODEX-TB.md` (ring version 2 with a kind, scale and angle; the compositor already reads
  version 1 and 2, lib test `the_trackpad_ring_reads_swipes_of_version_1_and_pinches_of_version_2`). Until then inject
  `pinch ...` in `.next`. AppKit's side (NSEvent magnify/rotate) is TERM8's.
- **Command-W / Command-Q for Mac applications**: a window is a Mac application's when its app id is a bundle's in the
  application folders, or its process runs from inside any `.app`, or it serves an app-menu socket (Firefox outside
  the catalogue was taken by the desk: CODEX-FX's finding). Not exercised with Firefox on the M1.
- **Dock catalog refresh** (CODEX-FX): done (above). Child surfaces (`wl_subcompositor`) and clipboard persistence:
  not done.

### 4. Missed blanks and interactive resize
- **Real-time wait** (patch 22): the last 3 ms before a commit waited for with `mach_wait_until` in the time-constraint
  scheduling class (`WMPACE precise wait: time-constraint policy ... rc=0` on the M1). **Measured: no gain**
  (`desk6-tests/m1/l1.txt`, 10 s phases, synthetic `stream`): missed blanks idle 4/572 and 5/566 off vs 6/569 and
  4/562 on; terminal flooding 6/529 off vs 7/573 on (0.7-1.2 % either way). The late frames had commit leads of
  0-2.5 ms, several of 2-2.5 ms - i.e. not only late wake-ups: the display's needed lead varies above the learned
  ~2.2 ms. So it is **off by default** (`CHONKSTEP_DARWIN_PRECISE_WAIT=1` or inject `precise on`). Missed blanks are
  **not 0**; next step: a lead learned per percentile of the late frames' leads, not the minimum.
- **Interactive resize latency: still not measured.** A press at the south-east handle of a desk terminal (the
  `list` step gives its centre, `resize_se`) followed by a 1 s pointer stream did not resize it (size unchanged, no
  `resize asked` samples), as in DESK5's three attempts. The owner's corner drag is the test.

### 5. Second display (compositor side)
- The wallpaper is fitted to an external output (scaled to cover, the middle kept; `scene.rs cover`), drawn by the
  GL renderer on that head and by the CPU renderer; dropped on unplug. Lib test
  `an_external_output_gets_the_wallpaper_fitted_to_its_shape`; wayland_outputs 2/2, GL second-head test pass.
- Not done: per-output client buffer scale, mirroring's rendering, freeing an unplugged head's GEM buffers
  (`ScanoutBuffer` keeps neither its EGL image nor its GEM handles, so a correct teardown needs those kept first;
  the buffers stay for a replug at the same mode, as DESK5 left it).

### Host tests (`runs/m1-desktop-010/desk6-tests/`)
wm-darwin lib 157/157; wayland_shm 19/20 (+ the base's autopilot failure: its expected menu rows), wayland_scale 3/4
(the base's failure), wayland_dmabuf 5/5, wayland_theme 3/3, wayland_outputs 2/2; chonk-shell 303/303 (+4 ignored);
chonk-dock 214/214 (+2 ignored). Every installed build was host-tested first.

### On the disk and how to go back
- `/usr/bin/chonkstep-darwin-desktop.next` = desk6-next-005 `2124825f…` (inject), `/usr/bin/chonkstep-darwin-desktop` =
  desk6-daily-005 `9203b446…`, `/usr/bin/chonkstep-dock` = desk6-dock-005 `fc0b2aad…` (hashes right after the copy).
  Files of before: `.before-desk6` (DESK5's 005 pair and dock-006), `.before-desk6-004` (my 002 pair),
  `.before-desk6-005` (my 004 pair, dock-001). `.safe` unchanged. New: `/usr/share/chonkstep/dock-pins`;
  pins file restored to the default set (before: `/private/var/db/desktop-state/chonkstep/dock.before-desk6`).
  `/private/var/db/desk6/` holds my test scripts (remove). Theme, wallpaper, scale untouched.
- Revert: `ln -f <path>.before-desk6 <path>.back && mv -f <path>.back <path>` for both compositor names and the dock,
  then `desk_m1.sh relaunch next`; pins: `mv dock.before-desk6 dock` in the state directory.
- Source: patches 20-23 in `integrations/puredarwin/terminal-app/compositor/` (README).

### For the owner, at the M1 (real keys and fingers)
1. Minimise a Chonkstep terminal and Activity Monitor (Command-M, or right-click the title > Minimize): one tile with
   the window's picture at the bottom left, nothing else; click it: the window comes back where it was.
2. Terminal.app: Command-M still crashes it until TERM8's c11 is installed - try again after their note.
3. Drag a dock launcher up or down: it follows your finger, the others make room, it stays where you drop it.
4. Drag one far away and let go at once: it goes back. Hold it away until "Remove from Dock" shows: then it goes;
   `desk_m1.sh dock-defaults` (or right-click > Options) brings the set back.
5. Right-click a launcher: Open / Options / Quit; a slightly wobbly click still opens the app.

### Taken from Finch (DESK6)
Nothing.
