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
- Three- or four-finger swipe left/right: next/previous Space, the desk following the fingers
  (**compositor side only**: see "The gesture" for what the trackpad driver still has to deliver).
  Control-Command + two-finger scroll does the same today (stand-in).
- Command-W closes the window, Command-Q quits the application (all its windows); for a Mac
  application (Terminal.app, Activity Monitor) Command-Q/W/M/H go to the application itself.
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
| Super-W | close window | Command-W | **observed** (t6-j); Mac app: goes to the app |
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
**Trackpad driver (not reached at the time of writing):** the kext (`m1-trackpad-child/multitouch`,
PDM1SPIHID in multitouch mode) decodes every contact but hands userspace only a synthesized pointer
(buttons, motion, one vertical wheel byte); three and four fingers are recognised by its libinput
port and then dropped (`M1TP_MT_G_IGNORED`). See the section "Kernel" at the end for what was done
afterwards.
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
Not done: recording, the capture toolbar, a selection outline that dims the rest.

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
the terminal brought forward on its Space. Not observed: launching Terminal.app or a new desktop
terminal from the dock; the dockapps (SOCK_SEQPACKET is not on Darwin: "dockapps are unavailable this
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
- On the M1 (10:45Z): `/usr/bin/chonkstep-darwin-desktop` = desk-daily-007 `56f4d073…`, `.next` =
  desk-next-007 `b25ea00d…` (inject), `.safe` = desk-daily-001 `b1104b75…`; before: `.before-desk`
  (cmdq-daily-001, `894bbf15…`), `.before-desk001`, `.before-desk006`, `.next.before-desk*`;
  `/usr/bin/chonkstep-dock` (`5f1baa06…`); `/usr/share/applications/*.desktop` (3);
  `/private/var/db/desktop-state/chonkstep/dock` (pins); `/usr/libexec/sf-session` (14653 bytes; the
  ones of before: `sf-session.before-desk`, `.before-apps`, `.old`).
  Revert: `ln -f <path>.before-desk <path>.back && mv -f <path>.back <path>` for the compositor and
  `.next`; `ln -f /usr/libexec/sf-session.before-desk ...` likewise; remove `/usr/bin/chonkstep-dock`
  (the session then starts no dock), the three `.desktop` files and the pins file.
- Cargo tests (host, `XDG_STATE_HOME` at a scratch directory): `cargo test --offline --locked -p wm-darwin
  --features desktop-shell`: lib 129/129; wayland_dmabuf 5/5; wayland_theme 3/3; wayland_shm 9/10
  (`autopilot_exercises_the_whole_desktop_against_a_real_window` fails); wayland_scale 3/4
  (`a_window_too_large_for_the_panel_at_a_scale_is_shrunk_and_gets_its_size_back` fails). **Both
  failures are the same on the base source** (`wm-darwin-cmdq-src`, run tonight; there the lib test
  `the_desk_is_drawn_at_every_scale_and_never_stretched` also failed: its expected menu lacked the
  "Session" row of 96effcf; the expectation is corrected). dock_host 1/1 (ignored by default; needs a
  host build of the dock). chonk-shell: `standalone_darwin_key_tests` 1/1 (the whole chonk-shell suite
  was not run).

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
