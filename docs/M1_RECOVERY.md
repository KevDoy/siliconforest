# M1 recovery qualification

`tools/m1_recovery.py` is the reusable M5-local form of the halt, hold, and one
`reboot serial` cycle. The default command is an offline plan. `--execute` is
Darwin arm64 and root only. This change did not SSH, open `/dev/cu.debug-console`,
run macvdmtool, or take the target lock. Root owns key setup, the installer, and
the one M5 execution.

Exit 0 means `OFFLINE_PLAN_ONLY` or `OBSERVATIONS_RECORDED`. Neither is a
hardware, m1n1, or XNU pass. `hardware_boot_observed` stays false.

## Fixed target

| Item | Value |
| --- | --- |
| SSH | `chrisk@10.1.1.127` |
| Identity | `/Users/chriskearney/darwin-lab-firstboot-007/controller-ssh-008/id_ed25519` |
| known_hosts | `/Users/chriskearney/darwin-lab-firstboot-007/controller-ssh-008/known_hosts` |
| Kernel | `7.2.11-6-1-ARCH` |
| Loader path | `/boot/efi/m1n1/boot.bin` |
| Original loader | `54fa07e4f5cbeecd90e67e335657c40b33c25aede7015a84afb1427333b4f923` |
| candidate004 now installed | `e4ead838bb87a04dd00909edf8a598f813056ee2f9611777a4a77c4bf7351e27` |
| macvdmtool | `/Users/chriskearney/darwin-lab-firstboot-007/native-control-src/macvdmtool` |
| Tool SHA-256 | `db6da3ff2f86d9663f748d975ce1ce988c82047014912018775b61f207b6ff4a` |
| Serial | `/dev/cu.debug-console` |
| Shared lock | `/Users/chriskearney/darwin-lab-firstboot-007/m1-controller-008.lock` |

Original loader (candidate004 now installed). Config must repeat those pins.
`bootloader_sha256` may be only the original `54fa…` digest or the installed
candidate004 `e4ead838…` digest, exact and lowercase. The remote command hashes the fixed loader path; it does not accept
a shell path from config. `private_directory` is the one free path: absolute,
no `..` or shell metacharacters, outside this repo and outside the key
directory. The runner creates that directory mode `0700` and the journal and
serial log mode `0600`. It refuses symlinks.

The controller lock is the shared file above, not a lock inside the private
directory. `--execute` takes it non-blocking and releases it on every exit,
including failures. If root still holds it for install or fallback, this runner
fails closed instead of resetting.

## SSH and observation

The client is `ssh -F /dev/null` with `BatchMode=yes`, `IdentitiesOnly=yes`,
`IdentityAgent=none`, `StrictHostKeyChecking=yes`, `RequestTTY=no`,
`ClearAllForwardings=yes`, `ConnectTimeout=8`, and the pinned identity and
known_hosts files. This program never reads or copies the private key. `ssh(1)`
reads `IdentityFile` only during `--execute`.

The readonly observation is one `sudo -n sh -c` script: kernel, NUL-separated
devicetree compatible names as separate `COMPAT=` lines, boot-id hash, loader
hash, the four old units, both watchdog properties, pid1 watchdog fd count via
`readlink` (the runner does not open `/dev/watchdog`), and root mount options.
`chrisk` cannot read the loader or `/proc/1/fd`; an unprivileged count would
report `WD_FD` as 0.

## Cycle

1. Refuse a journal that already records a native call, `native_requested`, or
   an unknown halt. A pre-halt abort may repeat only when the restore readback
   matched.
2. Confirm J293, T8103, the known kernel, the configured reviewed loader hash,
   and inactive old units.
3. Record the boot-id hash and watchdog microsecond values. Set
   `RuntimeWatchdogUSec` and `RebootWatchdogUSec` to 0 and require the pid1 fd
   count to be 0.
4. Append `halt_requested` and fsync the journal, then `sudo -n systemctl halt`.
   If that write fails, do not halt. SSH timeout or disconnect is the expected
   loss of the session. If SSH is still up after 20 seconds, restore the
   original watchdog values and abort with no native call.
5. Hold observed-down for 60 seconds. A return during that hold is
   `TARGET_UNKNOWN_NEEDS_INDEPENDENT_RESET` with no reset and no watchdog write.
6. Append `native_requested` with `native_calls` 1 and fsync, then one
   `macvdmtool reboot serial`. If that write fails, do not reset. A native
   timeout records `ACE state unknown` and does not retry. Wait up to 180
   seconds for a new boot id, the same kernel and loader hash, the original
   watchdog pair, and an `rw` root. If SSH is still down 25 seconds after the
   reset, run `/usr/sbin/arp -d 10.1.1.127` once with a 5 second bound and
   record the attempt and result. Host-key checks stay strict. There is no
   second reset.
7. `dfu`, `poweroff`, `nop`, `debugusb`, and a second native call are refused.

A native timeout or any error after `halt_requested` stays unknown. It is not
`ABORTED_BEFORE_HALT`, and it is not retried. Watchdog restore is true only
when that `busctl` command returns 0 and a later observation reads back the
original pair.

Serial capture is 115200 8N1 raw, read-only, non-controlling, and non-blocking.
The reader thread closes its descriptors in `finally` and is joined on the way
out. The log is private. It does not prove the CPU halted.

## Dependencies

The runner uses the Python 3 standard library plus `tools/lab.py`
(`ValidationError`, `read_json`, `regular_file`). No pip packages. `--execute`
also needs `ssh`, `sudo -n`, and the pinned macvdmtool binary. `python3
tools/m1_recovery.py --help` inserts the repo root on `sys.path` and imports
`lab` from `tools/` when it is launched as a script.

## Limits

- Not recovery from an arbitrary hang, DFU, power-off, or a dropped ACE
  connection. macvdmtool exit 0 is an ACE/VDM return, not a boot.
- No second reset from the same private journal after a native call.
- `kernel.panic` is unchanged. Watchdog changes are runtime properties.
- SSH is sampled about every 5 seconds.
- Host tests use fakes, a temporary lock file, and a PTY. They do not talk to
  the M1 or the M5.
- Root’s three earlier halt/reset cycles remain the ad hoc evidence in
  `reports/sprint-008/recovery-three-cycles.json`. This file does not add
  another hardware result.

```sh
python3 tools/m1_recovery.py --help
python3 tools/m1_recovery.py --config /absolute/recovery.json
PYTHONDONTWRITEBYTECODE=1 python3 -B -m unittest tests.test_m1_recovery -v
```

`--execute` is root’s command on the M5, after this lock is free and the
configured loader hash is the one actually installed.

## Reset against a native XNU (native-boot stream, boot `nb04`)

One `macvdmtool reboot serial`, sent by the per-boot controller copy of the native-boot
stream (`integrations/puredarwin/m1-native-boot/launcher-native.patch`, word `vdm-return`),
against the M1 running XNU without m1n1: kernel alive at an idle single-user shell, about
4.5 minutes after it started, no proxy USB device, no Linux. The tool returned 0
(`Rebooting target into normal mode... OK`), and a new Linux boot with the known kernel
answered within 30 s. This is one cycle against a **running** kernel. Against a panicked
or hung XNU the reset has not been run; the limit "not recovery from an arbitrary hang"
above still stands for those states.

