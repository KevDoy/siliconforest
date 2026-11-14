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
