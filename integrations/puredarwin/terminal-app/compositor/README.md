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

Builds:
```sh
B=integrations/puredarwin/chonkstep-darwin/tools/build_desktop_binaries.sh
JOBS=10 $B compositor <name> <source with patches 1 to 4>                              # the desktop: no inject module
JOBS=10 DESKTOP_FEATURES=test-inject $B compositor <name> <source with patches 1 to 4>   # the test build
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

On the M1 (14 November): `/usr/bin/chonkstep-darwin-desktop` is the build without the module
(sha256 `9115579a…`; strings: `WMINJECT absent` once, `WMINJECT armed` and `request.taken` not at
all), `.old-daily` the build of 12 November (`721574af…`), `.next` the test build (`810a1569…`).
Both marks are removed. Both builds are from `runs/m1-desktop-010/wm-darwin-apps-build-001/source`
(patches 1 to 4 applied); copies of the two binaries are in `runs/m1-desktop-010/wm-darwin-apps-keep/`.
`../../chonkstep-darwin/tools/build_desktop_binaries.sh` (the `DESKTOP_FEATURES` variable) is a
file of the storage stream that is not committed yet: the one line this job added to it is there
in the working tree.

Not run: the crate's own tests (`cargo test -p wm-darwin`); two test expectations were edited by
hand for the new Applications row.

## Terminal (`chonk-terminal-darwin`)
Base: the source of `terminal-application-build-008`.

| Patch | What | Built as |
|---|---|---|
| `terminal-application-daily.patch` | no end after 8 MiB of output or 1 MiB of input, `PATH`, `HOME=/var/root`, `LANG` for the shell (storage stream, step 7) | `terminal-application-daily-build-001` (installed on the M1) |
