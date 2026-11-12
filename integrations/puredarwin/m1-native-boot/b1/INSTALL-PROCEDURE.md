# Installing the SiliconForest stage on the M1's EFI partition

> **NOT AUTHORISED. NOT EXECUTED.** This is a procedure on paper for the owner to review.
> No step of it has been run, no script here runs it, and nothing in this directory writes
> to the M1. It changes one file and adds two on the EFI system partition (ESP). It does not
> touch NVRAM, the boot policy, Startup Security, stage 1, partitions, U-Boot or Linux.

Design and reasons: `DESIGN.md`, section 3. Read that first.

## 0. What must be true before this is even proposed for execution

1. The RAM proof has passed (`tools/sf_stage_boot.sh`): the stage boots the kernel by itself.
2. The PMU boot-error count has been read from m1n1 on this M1 and behaves as the design
   assumes: 0 after a clean Linux restart, 1 or more after a session ended by a reset.
3. The selector exists (about 60 lines in the stage-2 m1n1, not written yet), has been
   reviewed, and has been proven **from RAM**: the lab's bootstrap can chainload a candidate
   `boot.bin` into RAM exactly as it chainloads the pinned m1n1 today, with the marker's
   presence simulated, so that both of its branches have run on this M1 before either is
   on disk.
4. The stage for installation is a variant that restarts the machine when it fails, instead
   of falling back to the USB proxy, and that reads no storage (as today), or, if a RAM root
   is still wanted, reads exactly one named file from the ESP.
5. The owner is at the machine for the first boot of the new `boot.bin`, as for candidate004
   (`reports/sprint-008/candidate004-target-plan.json`:
   `first_persistent_boot_requires_owner_recovery_availability`).

## 1. Files

| on the ESP (`/boot/efi` in Linux) | from | size | role |
|---|---|---|---|
| `m1n1/boot.bin` | **replaced**: selector m1n1 + exactly the payloads of the current file (device trees, U-Boot, configuration lines), in the same order | about 10.3 MB | boots Linux unless told otherwise |
| `m1n1/boot.bin.darwin-lab-pre-sf-<sha12>` | **new**: byte copy of the `boot.bin` that was there | 10.2 MB | rollback |
| `m1n1/sf.bin` | **new**: `pdsf_pack.py pack --m1n1 m1n1-sf-stage.bin --kernel <collection>/kernel --bootargs '...'` | 14.5 MB | the SiliconForest stage |
| `m1n1/sf-next` | created by Linux when wanted, deleted by Linux at every boot | 0 | one-shot choice |

Already there and left alone: `m1n1/boot.bin.darwin-lab-original-54fa07e4f5cb` (the
distribution's original stage 2). ESP free space was 182 MB when candidate004 was planned;
this needs 25 MB.

The new `boot.bin` is built from the bytes of the installed one: the m1n1 part (up to the
footprint in its load header at 0x818) is replaced, everything after it is copied unchanged
(candidate004's installer validates the same split: `tools/m1_boot_install.py`, `footprint`,
`validate_payload`).

## 2. Procedure (in Linux on the M1, as root; every step checks before it writes)

1. `sha256sum /boot/efi/m1n1/boot.bin` equals the hash the new file was built from
   (candidate004: `e4ead838bb87…`). If not, stop: something else changed the file.
2. `findmnt /boot/efi` shows the ESP read-write, and `df` shows at least 40 MB free.
3. Copy the three files to the ESP under temporary names
   (`boot.bin.sf-new`, `sf.bin.new`, the backup under its final name), `fsync` each and the
   directory, drop the page cache for them and re-read their SHA-256 from the disk. The
   existing installer does exactly this for one file (`tools/m1_boot_install.py`; its
   reviewed logic is to be reused, not rewritten).
4. `mv sf.bin.new sf.bin`, then **last**: `mv boot.bin.sf-new boot.bin`, `fsync` the
   directory. A rename inside one FAT directory is the smallest window this filesystem
   offers; a power cut inside it can leave no `boot.bin`: see 5, "no Linux".
5. Re-read all hashes. Do not create `sf-next`. Restart: Linux must come back, with the new
   stage 2's version line on the serial console. Only then is the selector considered
   installed.
6. Install the Linux side: a boot unit that deletes `/boot/efi/m1n1/sf-next` if it exists
   (early, every boot), and a command `sf-boot-once` that creates it and restarts.

## 3. How Linux stays the default, and how SiliconForest is chosen

- No marker: the selector reads the PMU boot-error count and goes on to U-Boot and Linux.
- `sf-boot-once` in Linux: creates `m1n1/sf-next`, restarts. The selector sees the count
  at 0 and the marker, loads `m1n1/sf.bin` and runs it.
- The session ends (today: by a reset; later: by the kernel's own restart). The next boot
  finds the count above 0 and boots Linux whatever the marker says; Linux clears the count
  and deletes the marker.
- A SiliconForest that hangs: hold the power button until the machine is off, press it
  again: Linux. Nothing to repair.
- A package update that runs `update-m1n1` replaces `boot.bin` with the distribution's
  (and keeps ours as `boot.bin.old`): Linux boots, SiliconForest is no longer reachable
  until step 4 is repeated. That is the safe direction; do not disable `update-m1n1`.

## 4. Rollback

In Linux: `cp m1n1/boot.bin.darwin-lab-pre-sf-<sha12> m1n1/boot.bin.rb && sync && mv
m1n1/boot.bin.rb m1n1/boot.bin && sync`, check the hash, delete `sf.bin` and `sf-next`.
The same command with `boot.bin.darwin-lab-original-54fa07e4f5cb` goes back to the
distribution's stage 2 without the lab's USB window.

## 5. What a failure looks like, and the way back

| what is seen | what it is | recovery |
|---|---|---|
| Linux boots, `sf-boot-once` gives Linux again | count was not 0, `sf.bin` missing or refused (too large, bad CRC) | serial log of stage 2 says which; fix the file from Linux |
| SiliconForest's stage prints `PDSF: ...` and the machine restarts into Linux | the stage refused or failed (its line says why) | none needed |
| black screen or frozen logo after choosing SiliconForest | kernel hang | hold power to switch off, power on: Linux |
| **no Linux**: m1n1 logo, then "No valid payload found" / "Running proxy..." on the screen | the new `boot.bin` is damaged or its selector is wrong | recoveryOS, below. (The proxy on USB lets the controller reset the M1; it does not repair the file.) |
| **no Linux**: stage 1 shows the m1n1 logo and stops ("failed to load" on serial) | `boot.bin` missing or unreadable | recoveryOS, below |

**From recoveryOS / 1TR** (hold the power button at power-on until "Loading startup
options", choose Options; this is Apple's paired recovery and needs no boot-policy change
to *use*):

1. Utilities, Terminal.
2. `diskutil list`: find the EFI partition of the Linux install (type `EFI`, about 500 MB,
   the one that is not Apple's first partition; its volume name is usually `EFI - <NAME>`).
3. `diskutil mount diskNsM`; `cd /Volumes/EFI*/m1n1`.
4. `cp boot.bin.darwin-lab-pre-sf-<sha12> boot.bin` (or `…-original-54fa07e4f5cb`);
   `rm -f sf-next`; `sync`; `diskutil unmount diskNsM`; restart.

Nothing in this procedure can affect stage 1, the boot policy or recoveryOS itself, because
none of them is on the ESP. The worst case of a wrong `boot.bin` is therefore "Linux does
not start until one file is copied back".

## 6. What this procedure deliberately does not contain

- No command that writes to the M1 is given in executable form, and no script in this
  repository performs any of the steps.
- No NVRAM variable, `bputil`, `kmutil`, `csrutil`, Startup Security Utility, DFU or
  partition change. If a later design wants SiliconForest in Apple's own boot picker (its own
  APFS stub with its own stage 1), that is a boot-policy change and a different document.
