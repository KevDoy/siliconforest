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
