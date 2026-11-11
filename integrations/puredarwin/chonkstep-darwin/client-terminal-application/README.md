# chonk-terminal-darwin

Separate ordinary terminal application derived from the frozen native-startup
observer, whose 14 original source hashes remain intact. The app uses a real
owned `/bin/sh -i +m` PTY, compositor XKB keymap, an xterm emulation (TERM=xterm-256color, see below), CPU font
renderer, SHM buffer ownership and server-side decorations. The former response
recognizer and startup-only title/readiness policy are removed.

The agreed launch/status interface is in [APPLICATION_CONTRACT.md](APPLICATION_CONTRACT.md).
The real desktop session supplies absolute socket/record paths, a unique lowercase
48-hex instance ID, finite remaining lifetime and private Unix stream fd 3.
The terminal forks its owned PTY child before opening Wayland; that child closes
fd 3 and every other descriptor above stdio except its CLOEXEC exec-status pipe.
Root owns the separate pinned-SDK C launch adapter; this crate does not implement
or guess Darwin spawn attribute layouts.

Readiness requires checked shell setup/exec, XKB map, SSD negotiation and an actual
frame callback; it is independent of focus or held keys. Enter's held-key array
never becomes typed input. Accepted PTY writes remain queued in order after
focus leave. Normal close, shell EOF, signal and deadline stop cleanly after owned
cleanup even before READY. Final records retain unsent input counts and real raw
wait status. A status-channel/final-record failure is an application failure.

Limits are explicit: the emulation is what the section below lists, no detached
or background jobs, and no foreground-group relaxation. Input is bounded to
16,384 lifetime bytes and PTY output to the inherited 1 MiB budget (one bounded
read may expose quota overflow before shutdown). The supplied 1–120 second
lifetime bounds application operation; its existing 250 ms TERM grace/2 second
reap budget and up to 250 ms status flush must fit the session's reserved shutdown
time. An OS syscall that stops returning still requires root's outer supervisor.
The inherited shell environment remains explicit and fixed; GUI environment or
launch expansion requires coordination with the session contract.

Host checks (Linux only, synthetic input, no target execution):

```
cargo test --manifest-path Cargo.toml --locked --offline --target-dir target-host
cargo build --manifest-path Cargo.toml --locked --offline --target-dir target-host
```

Tests need host xkeyboard-config/libxkbcommon and Python 3 for the real foreground
group fixture. Test-only wayland-server 0.31.14 drives an actual anonymous local
Wayland client connection, XKB fd, SSD configure/ack, SHM fd, buffer release and
frame callback. Its presentation acknowledgement is a wire fixture, not a renderer
or physical scanout witness. Cases cover unfocused startup, held launch keys,
typed bytes followed by Leave, close/EOF before READY, disconnect cleanup,
real shell descriptor exclusion, setup/exec failures, resize and TERM/KILL/reap.
Partial writes, full/broken status channels and private-record collisions have
separate bounded tests. The session must still qualify complete launch-chord
ownership, two-window interaction and actual target execution.

Host tests do not establish Darwin execution or physical input. Native keyboard
and pointer observations belong to the preserved earlier receipts; raw HID wire
correlation remains separate. Root owns SDK build, artifact binding and hardware.

## Long sessions (10 November)

`src/` here is now the source the deployed binary is built from: the fd-boundary revision that
`runs/m1-desktop-010/terminal-application-build-002` was built from (its `FD_BOUNDARY_CONTRACT.md`,
`fd-boundary.patch` and `test_fd_mutations.py` are copied alongside), plus the change in
`long-session.diff`, made for interactive desktop sessions of up to four hours:

- `--seconds` accepts 1..14400 (was 1..120);
- `MAX_RX` (PTY output, which is also written to the records directory in the RAM root) is
  8 MiB per terminal (was 1 MiB); `MAX_TX` (typed input) is 1 MiB (was 16 KiB).

A terminal still ends itself when it passes either quota. The binary built from this source is
`runs/m1-desktop-010/terminal-application-build-003/target/aarch64-apple-darwin/release/chonk-terminal-darwin`
(same imports and size as build-002; 43 host tests pass):

    CARGO_TARGET_AARCH64_APPLE_DARWIN_LINKER=$PWD/../darwin-linker.py \
      cargo build --offline --locked --release --target aarch64-apple-darwin --target-dir <run>/target

`source-validation.json`, `host-tests.log` and `revisions/` describe earlier revisions and were
not regenerated for this one.

## Terminal emulation and scrollback (10 November, build-004)

The TERM=dumb grid (`src/grid.rs`) is gone. `src/term.rs` wraps the `vt100` crate
(0.18.1, parser `vte` 0.15.0; with `arrayvec` 0.7.8, `itoa` 1.0.18, `memchr` 2.8.3,
`unicode-width` 0.2.3 — all pinned in `Cargo.lock`, built `--offline --locked` from
the cargo registry cache like the other dependencies; no platform code in any of them).

* Screen: cursor addressing, erase, insert/delete, scroll regions, SGR with the 16,
  256 and 24-bit colours, bold, dim, underline, inverse, the alternate screen (1049/47),
  cursor hide/show, wide characters. `CSI s`/`CSI u` are mapped to save/restore
  cursor (toybox `vi` measures the screen with them). The terminal answers `CSI 5n`,
  `CSI 6n` (cursor position), `CSI c` and `CSI > c`.
* Environment: `TERM=xterm-256color`, `COLORTERM=truecolor`. The RAM root has no
  terminfo; the boot wrappers stage `../terminfo/xterm-256color` (ncurses 6.6, sha256
  1eb6ddea…) as `/usr/share/terminfo/x/xterm-256color` and `/usr/share/terminfo/78/…`.
* Keys (`src/keys.rs`): arrows (CSI, or SS3 while the program has application cursor
  keys on, `CSI 1;m X` with modifiers), Home/End, Insert/Delete/PageUp/PageDown,
  F1–F12, Shift+Tab, Alt as an ESC prefix.
* Resize: a configure changes the grid; the PTY gets TIOCSWINSZ and the foreground
  program SIGWINCH. Bounds: 20–400 columns, 6–150 rows.
* Scrollback: 5000 lines. Two-finger scroll (wheel events, three lines a detent) and
  Shift+PageUp / Shift+PageDown (a page less one line) move the view; typing returns
  to the bottom. On the alternate screen wheel steps are sent to the program as
  Up/Down arrow keys instead.
* Drawing: only rows whose content changed are redrawn, written to the shm file and
  damaged.
* State for checks from outside: `<records>/term-live.txt`, rewritten (rename) at most
  every 200 ms when it changed: a `TERMSTATE cols= rows= alternate_screen= application_cursor=
  bracketed_paste= cursor= cursor_hidden= scrollback_offset= scrollback_lines= unsupported=`
  line, then the text on screen. The compositor's autopilot reads it.

Open: copy/paste. The compositor does not advertise `wl_data_device_manager`, so there
is no selection to paste from; the emulator tracks bracketed-paste mode (reported in
TERMSTATE) but nothing pastes. There is no mouse selection and no mouse reporting to
programs. Blink and italic are parsed but not drawn differently.

Host tests (`cargo test --offline --locked`, 47): emulator unit tests, and
`host_full_screen_programs_draw_and_exit_cleanly`, which runs the host's real `less`
and `vim` on a PTY through the emulator.
