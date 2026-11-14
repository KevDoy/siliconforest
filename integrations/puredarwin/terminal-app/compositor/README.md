# The compositor and terminal the M1's disk runs: where their source is

The Chonkstep repository is local only (`sources/chonkstep-darwin`; it is never pushed). What the
SiliconForest desktop runs is built from copies of it under `runs/m1-desktop-010/<build>/source`
(`../../chonkstep-darwin/tools/build_desktop_binaries.sh`). The changes made in those copies are
kept here as patches, in the order they apply, so that a lost build tree loses nothing.

## Compositor (`chonkstep-darwin-desktop`, crate `wm-darwin`)
Base: branch `wm-darwin` of the local repository at `96effcf` ("Session > Restart and Shut Down in
the desktop menu ...").

| # | Patch | What | First built as |
|---|---|---|---|
| 1 | `wm-darwin-daily.patch` | the session's length bound (a hundred years), the storage stream's daily-use changes (`docs/roadmap/issues/SF-R-storage.md`, step 7) | `wm-darwin-daily-build-001` (the compositor installed on the M1 since 13 November) |
| 2 | `wm-darwin-inject.patch` | the inject module: requested key and pointer records and captures of the screen, for tests | `wm-darwin-inject-build-002` |
| 3 | `wm-darwin-popups.patch` | client popups (`xdg_popup`), frame state told to clients, fitted sizes: the Cocoa stream's commits `a447cef..f0955fb` of branch `wm-darwin-cocoa` (local), unchanged. Every AppKit menu, pop-up button and tool tip is such a popup | `wm-darwin-apps-build-001` |
| 4 | `wm-darwin-applications.patch` | the desktop menu's Applications submenu (one row per bundle in `/Applications` and `/Applications/Utilities`; the path goes to the session's pipe, `open --listen` starts it: the compositor starts no program), and the inject module behind the feature `test-inject` | `wm-darwin-apps-build-001` |
| 5 | `wm-darwin-mac-command-q.patch` | Command-Q goes to the focused toplevel when its app id is the bundle identifier of a bundle in `/Applications` (a Mac application quits itself, with its questions, as on macOS); elsewhere it stays the desktop's "quit application" | `wm-darwin-cmdq-build-001` (test), `wm-darwin-cmdq-daily-001` |
| 6 | `wm-darwin-client-states.patch` | a client's `xdg_toplevel` set/unset_fullscreen, set/unset_maximized and set_minimized go to wm-core (one configure with size and state; requests made before the first buffer are kept until the window maps); `wm_capabilities`; inject lines `hwheel`, `scroll`, `swipe` (DESK, night of 14 November) | `desk-next-001`, `desk-daily-001` |
| 7 | `wm-darwin-desktop-night.patch` | the desktop of the night of 14 November (DESK, `docs/roadmap/issues/SF-Z-desktop-night.md`): the owner's Linux key table on the standalone desk (Omarchy keymap with Command as Super, plus the Mac profile's free chords) and the Spaces profile; three/four-finger swipes between Spaces that follow the fingers (wm-core's Linux recognizer and spring); screenshots (keys, `--screencapture`, clipboard) and `--desktop-request terminal` over a uid-checked socket; `zwlr_layer_shell_v1`, `zwlr_foreign_toplevel_management_v1`, `zxdg_output_manager_v1` (the Chonkstep dock), `xdg_activation_v1`; `/System/Applications` in the Applications rows; Darwin socket shims in `dock/crates/chonk-dock-proto`; the host test `tests/dock_host.rs` | `desk-next-007`, `desk-daily-007` |
| 8 | `wm-darwin-trackpad-swipes.patch` | the compositor reads the trackpad driver's swipe ring (`sysctl kern.pd_trackpad_swipe`, `m1-trackpad-child/multitouch/PDM1SPIHIDMultitouch.inc`) once a frame and turns each event into the swipe of patch 7; nothing happens with a kernel without it | `desk-next-008`, `desk-daily-008` |

Builds:
```sh
B=integrations/puredarwin/chonkstep-darwin/tools/build_desktop_binaries.sh
JOBS=10 $B compositor <name> <source with patches 1 to 5>                              # the desktop: no inject module
JOBS=10 DESKTOP_FEATURES=test-inject $B compositor <name> <source with patches 1 to 5>   # the test build
```

### The inject module is a test instrument; how it is kept out of daily use
1. Build: the module is compiled only with the feature `test-inject`. The compositor installed as
   `/usr/bin/chonkstep-darwin-desktop` is built without it: it contains no code that reads requests,
   and says `WMINJECT absent` if a session asks for it.
2. Session: `m1-storage/system/sf-session` gives the compositor an inject directory only when root
   has made `/private/var/db/desktop-inject`, and runs the test build
   (`/usr/bin/chonkstep-darwin-desktop.next`) only in the one boot that finds
   `/private/var/db/desktop-next-try` (the mark is renamed before the compositor starts).
3. The directory is in the session's runtime directory, mode 0700, root's.
A fresh root has neither mark and no `.next` file.

On the M1 (14 November, later): `/usr/bin/chonkstep-darwin-desktop` is `wm-darwin-cmdq-daily-001`
(patches 1 to 5, no inject module, `894bbf15…`), `.next` is `wm-darwin-cmdq-build-001` (with it,
`6136286b…`); the builds of before are `.before-cmdq` and `.next.before-cmdq` (copies in
`runs/m1-desktop-010/wm-darwin-cmdq-keep/`; the source is `runs/m1-desktop-010/wm-darwin-cmdq-src`).
Earlier the same day: `/usr/bin/chonkstep-darwin-desktop` was the build without the module
(sha256 `9115579a…`; strings: `WMINJECT absent` once, `WMINJECT armed` and `request.taken` not at
all), `.old-daily` the build of 12 November (`721574af…`), `.next` the test build (`810a1569…`).
Both marks are removed. Both builds are from `runs/m1-desktop-010/wm-darwin-apps-build-001/source`
(patches 1 to 4 applied); copies of the two binaries are in `runs/m1-desktop-010/wm-darwin-apps-keep/`.
`../../chonkstep-darwin/tools/build_desktop_binaries.sh` (the `DESKTOP_FEATURES` variable) is a
file of the storage stream that is not committed yet: the one line this job added to it is there
in the working tree.

Not run: the crate's own tests (`cargo test -p wm-darwin`); two test expectations were edited by
hand for the new Applications row.

### Night of 14 November (DESK)
Source: `runs/m1-desktop-010/desk-src` (`wm-darwin-cmdq-src` + patches 6 to 8; a local git with one
commit per patch; the patches applied to a fresh copy of `wm-darwin-cmdq-src` give the same tree).
Builds as above (`desk-next-NNN` with `DESKTOP_FEATURES=test-inject`, `desk-daily-NNN` without).
The dock: `../../desktop/tools/build_dock.sh <name> <same source>` (binary `chonk-dock`, installed as
`/usr/bin/chonkstep-dock`; its launcher items are `../../desktop/system/applications/*.desktop` in
`/usr/share/applications` and `../../desktop/system/dock-pins` as `$XDG_STATE_HOME/chonkstep/dock`).
Installing and restarting: `../../desktop/tools/desk_m1.sh` (`install`, `put`, `relaunch next|daily`).
Every build is first host-tested: `cargo test --offline --locked -p wm-darwin --features desktop-shell`
(the lib tests construct the standalone desktop session; desk-002 ended at start on the M1 because
nothing had) and, with a host build of the dock, `CHONK_DOCK_BIN=... cargo test ... --test dock_host -- --ignored`.
`/usr/bin/chonkstep-darwin-desktop.safe` is a build that ran on the M1 (desk-daily-001): the session
starts it when a compositor ends by itself within 20 s (`m1-storage/system/sf-session`).

## Terminal (`chonk-terminal-darwin`)
Base: the source of `terminal-application-build-008`.

| Patch | What | Built as |
|---|---|---|
| `terminal-application-daily.patch` | no end after 8 MiB of output or 1 MiB of input, `PATH`, `HOME=/var/root`, `LANG` for the shell (storage stream, step 7) | `terminal-application-daily-build-001` |
| `terminal-click-no-highlight.patch` (on top of the daily source) | a click no longer leaves a one-cell white block: a press that has not left its cell selects nothing (the motion events that came with a press made a selection of that cell); a drag still highlights | `terminal-click-build-001` (installed on the M1 since 14 November; the binary of before is `/usr/bin/chonk-terminal-darwin.pre-y2`; source `runs/m1-desktop-010/terminal-click-src`) |
