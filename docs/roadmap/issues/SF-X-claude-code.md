## Goal
The Claude Code CLI (Anthropic's vendor binary for macOS arm64, a Bun program) runs on SiliconForest on the M1: interactively in a terminal and with `-p`.

## Status (13 November 2026)
**It does not.** On the M1 `claude --version` and `claude --help` work; everything that runs the program proper (`claude`, `claude doctor`, `claude -p …`) is killed at once with exit status 137 and no output. In QEMU the same file reaches its login screen (`SF-S-homebrew.md`, runs c2 to c5; `SF-R-storage.md`, run u2 for `--version` on the M1's root image). The owner's decision: the cause is to be fixed in the kernel, as its own job. Nothing here is a fix.

The binary on the disk root: `/usr/local/lib/claude-code/2.1.322/claude` (323,702,464 bytes, sha256 `a9d23794…`, the checksum of the vendor's manifest; put there over SSH and compared by SHA-256 on the M1, boot dr39), `/usr/local/bin/claude` -> `../lib/claude-code/2.1.322/claude`. Nobody signed in; no credential was ever in the system (the scripts count the environment lines naming a key or token: 0).

## Evidence (boots dr42, dr44, dr45; installed chain, native, root on the disk)
Console (`runs/m1-desktop-010/pcie-enum-attempt-dr44/guest/console.bin`), for every killed start:
```
CODE SIGNING: cs_invalid_page(0x117bb8000): p=349[claude] final status 0x20010000, allowing (remove VALID) page
Failed to send exception EXC_CORPSE_NOTIFY. error code: 5 for pid 349 proc=? etype=10 subcode=0x700000002
```
- `etype=10` is EXC_CRASH; the status the shell sees is 137 (SIGKILL). `subcode=0x700000002` is the same for every killed start, in three boots.
- **What the subcode means is not established.** `osfmk/kern/task.c` prints the value it was handed. I first read it as an exit reason (namespace 7, code 2) and told the coordinator "code 2, invalid page"; that reading is not supported by anything: namespace 7 is `OS_REASON_LIBXPC` in this tree's `bsd/sys/reason.h`, which makes no sense here. It may as well be the original Mach exception's subcode (for EXC_BAD_ACCESS: the faulting address). For comparison, the tree's `sftp-server`, which dies with SIGBUS (status 138), prints `subcode=0x20000000a`.
- The `cs_invalid_page … allowing (remove VALID)` line is not the kill: node prints the same line (`p=376[node] final status 0x20000000, allowing`) and runs.

`runs/sf-userland/dr44-ssh.log` (script `runs/sf-userland/claude-diag.sh`, each command with stdout and stderr kept, 25 s limit):
| Command | Result |
|---|---|
| `claude --version` | rc 0, `2.1.322 (Claude Code)` |
| `claude --help` | rc 0, prints the usage |
| `claude doctor` (no terminal) | rc 137, no output |
| `claude -p hello` | rc 137, no output |
| `BUN_JSC_useJIT=0 claude -p hello` | **rc 1, not killed**: `ReferenceError: SharedArrayBuffer is not defined … Bun v1.4.4 (macOS arm64)` (Bun without its JIT lacks SharedArrayBuffer, which the program needs: not a way to run it) |
| `claude` in tmux (100x30), plain / `NO_COLOR=1 TERM=vt100` | `EXIT=137` at once, no screen, nothing on stderr |
| `BUN_JSC_useJIT=0 claude` in tmux | `EXIT=1`, the same ReferenceError |
| `node -e` with a 30-million-iteration loop (V8's JIT) | rc 0 |
So: the kill needs JavaScriptCore's JIT to be on. Node's JIT (V8) is not affected.

## The signatures (read with `homebrew/hb_macho.py`)
| | Claude Code (vendor) | node (Homebrew bottle) |
|---|---|---|
| CodeDirectory flags | `0x10000` (hardened runtime) | `0x2` (ad hoc) |
| CMS signature | yes (Developer ID, 9,054 bytes) | no |
| Entitlements | XML and DER blobs: **`com.apple.security.cs.allow-jit` = true**, `com.apple.security.cs.allow-unsigned-executable-memory` = true, `com.apple.security.cs.disable-library-validation`, `com.apple.security.device.audio-input`, `com.apple.security.automation.apple-events` | none |
| Process status in the kernel's line | `0x20010000` | `0x20000000` |

## What was tried, and failed (boot dr45)
A copy of the binary with the hardened-runtime bit cleared in its one CodeDirectory (`m1-storage/userland/sf_cs_runtime.py`; one byte differs, sha256 `0d06a4e5…`), on the disk as `/usr/local/lib/claude-code/2.1.322/claude.sf`. Its process status is then node's (`p=395[claude.sf] final status 0x20000000`), and **it is killed exactly as before** (`doctor` 137, `-p` 137, tmux `EXIT=137`, the same `subcode=0x700000002`). So the hardened-runtime flag alone is not the cause. `/usr/local/bin/claude` was left pointing at the vendor's file (the script switches the link only if the copy reaches its first screen). **`claude.sf` is still on the disk and is of no use: remove it** (`rm /usr/local/lib/claude-code/2.1.322/claude.sf`, 309 MB). There is no stopgap in place, hence nothing to revert; should a later job link `/usr/local/bin/claude` to a changed copy, the way back is `ln -sfn ../lib/claude-code/2.1.322/claude /usr/local/bin/claude`.

## Not established
- The kill site in XNU, and what `subcode=0x700000002` is (above). To find out: the corpse's exception codes and exit reason (`proc_prepareexit`, `task_deliver_crash_notification`; a line printed from there, or `debug=` settings that keep the corpse), or the same start under `dtruss`-like tracing if the tree has one.
- Why QEMU is not affected. The emulated CPU (`neoverse-n2`) has none of Apple's JIT machinery (the per-thread write/execute switch of `pthread_jit_write_protect_np`, MAP_JIT pages), and the `virt` kernel is another build (`DEBUG_ARM64_VIRT`); the M1's is `DEVELOPMENT_ARM64_T8103`, started with `sprr=1 amx=1` in the stage's configuration. Which of these matters is not known.
- Whether JavaScriptCore asks for MAP_JIT memory and whether this kernel grants it without a validated allow-jit entitlement (the system has no signature validation at all: the entitlements above are never read).
- Whether Bun's JIT works at all on the M1 here: a Bun bottle (`brew install bun` is not in homebrew-core; `oven-sh/bun` is a tap) or another JavaScriptCore program would separate "JavaScriptCore's JIT" from "this binary".

## Files
- `runs/sf-userland/claude-diag.sh` (the table above), `runs/sf-userland/claude-sf.sh` (the copy's test and the conditional link), `runs/sf-userland/claude-tui.sh` (first screen in tmux); sent with `m1-storage/tools/sf_userland_push.sh ssh sh -s < <script>` during a bench boot of the installed chain (`SF-R-storage.md`, "Step 7"). `runs/` is not in git: the three scripts are short and are reproduced by the table.
- Logs: `runs/sf-userland/dr42-ssh.log`, `dr44-ssh.log`, `dr45-ssh.log`; consoles under `runs/m1-desktop-010/pcie-enum-attempt-dr4{2,4,5}/guest/console.bin`.
- `integrations/puredarwin/m1-storage/userland/sf_cs_runtime.py`: the one-bit copy (kept as a diagnostic; it did not help).
