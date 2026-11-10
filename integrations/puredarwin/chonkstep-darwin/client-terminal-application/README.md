# chonk-terminal-darwin

Separate ordinary terminal application derived from the frozen native-startup
observer, whose 14 original source hashes remain intact. The app uses a real
owned `/bin/sh -i +m` PTY, compositor XKB keymap, bounded TERM=dumb grid, CPU font
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

Limits are explicit: TERM=dumb, no xterm/editor compatibility promise, no detached
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
