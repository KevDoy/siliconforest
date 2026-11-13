# Installing SiliconForest on the M1's disk: the kit and the steps

> **Nothing in this document has been executed on the M1's disk.** The kit is built and tested
> on the workstation and, as far as that is possible without the partitions, on the bench from
> RAM ("What is proven" at the end). The steps that write (4 and 6) are the owner's.
> This replaces sections 4 to 8 of `../b1/INSTALL-PROCEDURE.md`; its sections 1 to 3 (what
> exists afterwards, where the space is, what the stub needs from Apple) still describe the result.

## The route

The Asahi Linux installer itself (source at commit `e93743d`, the code that made the Linux
entry on this machine), run on macOS with **our** installer data and package. Its code makes
the APFS container with its four volumes, the stub macOS with its paired recoveryOS, the FAT
partition, and the boot object; `step2.sh` in recovery sets the boot policy. What is ours:

| Piece | File in the kit | What it is |
|---|---|---|
| OS entry | `installer/installer_data.json` | one entry, "SiliconForest": boot object `m1n1.bin`, next object `sf/boot.bin`, one partition `SFBOOT` (type EFI, FAT32, 500 MiB, filled from the package's `esp/` tree) |
| Stage 1 | `installer/boot/m1n1.bin` | m1n1 at the lab's pin, `RELEASE=1 CHAINLOADING=1`, our logo, the installed variant (below). The installer appends `chainload=<PARTUUID of SFBOOT>;sf/boot.bin` and puts the result into the stub as the boot object |
| Package | `installer/os/siliconforest.zip` | `esp/sf/boot.bin` (stage 2 + configuration + kernel collection, 14.4 MB) and the icon |
| Icon | `installer/logo.icns` | from `branding/siliconforest-icon.svg` |
| Menu | `installer/main.py` | the installer's, with one hunk (`installer/main-menu.patch`): only "install into free space", "repair an incomplete installation" and "quit" are offered. Resizing a container, replacing the m1n1 of another stub, rebuilding firmware and other disks are not on the menu |

Everything else in `installer/` is the installer's source, byte for byte.

Two things differ from the earlier plan, on purpose:
- **SFBOOT is 500 MiB (128,000 blocks), not 512.** The free space is 2,896.16 MiB and the stub
  takes 2,384; 512 would leave 0.16 MiB for whatever alignment `diskutil` applies to two
  partitions whose space starts at an odd block. 500 MiB is the size the installer has always
  used for its FAT partition; about 12 MiB after it stay unallocated. The file is 14.4 MB.
- **SFBOOT has the partition type "EFI System"**, because that is the only type for which the
  installer records the partition and writes `chainload=` (`osinstall.py`: `if ptype == "EFI"`).
  A second EFI-typed partition is what every second Asahi install on one disk has. Linux's own
  stage 1 names its EFI partition by PARTUUID and so does `/etc/fstab` (check in step 1).

**The installed variant of m1n1** (`patches/0001`, build flag `SF_INSTALLED`, both stages):
where m1n1 would open its USB proxy ("No valid payload found") or restart the machine (a
panic, an exception), it turns the screen log on, prints why, and stops:

    SiliconForest: stopped (<reason>).
      Nothing on the disk was changed by this.
      Hold the power button until the machine is off. A short press then starts
      the default system; holding the button at start shows the startup picker.

No USB device appears, nothing restarts by itself, nothing is written. (The log above that
message is m1n1's own and includes its copyright line, "The Asahi Linux Contributors"; it is
visible only in this failure case. A normal start shows the mark on black and nothing else.)

## What an unattended start does when something fails

| Failure | What happens | What the owner sees | Way out |
|---|---|---|---|
| stage 1 finds no partition with that PARTUUID, no `sf/boot.bin`, or cannot read it | `sf_stop` | the m1n1 log and the message above | hold power; start again (default system), or the picker |
| stage 2 refuses (`PDSF: …`: a checksum, a root other than the one it was built for, no room, no seed from the SEP) | `sf_stop` | the reason line, then the message | the same; then repair `sf/boot.bin` from Linux (`linux/sf-bootbin-install.sh`, `--rollback`) |
| m1n1 itself panics | `sf_stop` instead of its restart | the message | the same |
| the kernel starts and finds no root (partition table changed, filesystem gone, the driver's fence refuses) | the kernel waits for the root. Seen once on the bench for four minutes (boot dr03): no restart, nothing written. `IOSetRecoveryBoot` is a no-op in this kernel (`kernel-patches/0001`), and there is no NVRAM driver: boot policy and NVRAM cannot be changed from here. **Not observed beyond four minutes** | the mark, nothing else | hold power |
| the kernel panics | it spins (boot nb10: "machine will reset or spin", it spun); the report goes to the serial port only | a frozen picture | hold power. The root was mounted read-write: its journal is replayed by SiliconForest at its next start (boots dr13 to dr16); Linux never mounts it |
| the desktop never appears, or hangs | nothing by itself | a black or frozen screen | hold power; as above |

None of these restarts the machine, so none can loop. After a forced power-off the PMU holds
one boot error until Linux boots and clears it (SiliconForest only reads it). **Keep Linux the
default system (step 8)**: then every restart and every power cycle after a failure lands in
Linux, the count never passes 1, and the firmware's own reaction to repeated failed starts
(not known to this lab) is never reached.

## What the installer does outside the two new partitions (read in its source)

- `diskutil addPartition` twice: the partition table, both copies, gains two entries. macOS
  may reorder the table's entries by disk position (Linux's `nvme0n1p4…` would become `p6…`; the
  lab finds partitions by identifier).
- `bless --setBoot` on the new volume (`main.py` `bless()`): **the default startup volume
  becomes SiliconForest.** This is not optional: recovery's step only works from the recoveryOS
  paired with the default volume. It is put back in step 8.
- In recovery (`step2.sh`): `bputil -nc -v <volume group>` and `kmutil configure-boot` create
  the boot policy of **this** volume group in the system's `iSCPreboot`. Other systems'
  policies are separate files and are not touched.
- It reads (does not write) macOS's Preboot volume for the list of users who may authorise
  (`AdminUserRecoveryInfo.plist`), and mounts the Preboot/Recovery/System volumes of every APFS
  container to list the installed systems. If macOS's own list of admin users is stale it runs
  `diskutil apfs updatePreboot` on **macOS's** volume (`check_cur_os`); that is Apple's
  maintenance command and the only write to an existing system I found.
- Not done with our data: no firmware collection, no `asahi/` directory, no report to anyone
  (`REPORT` is unset by `sf-install.sh`), no resize, nothing on Linux's stub, EFI partition,
  `/boot` or root, nothing on our root partition.
- Downloads: (a) by `sf-install.sh`, once, the Asahi installer package from
  `cdn.asahilinux.org/installer` (their bootstrap's address), used **only for its bundled
  Python**; (b) by the installer, single files out of Apple's macOS 13.5 IPSW on
  `updates.cdn-apple.com` (the version it picks for this machine's firmware, as for the Linux
  entry): manifests, the boot firmware named in the manifest and the recovery image
  (`arm64eBaseSystem.dmg`). Sizes not measured here: tens of MB for (a), several hundred MB up
  to about 1 GB for (b). Plain HTTPS requests; no account, no identifier of the machine or the owner.

## Steps

Numbers used below: free space = blocks 208,669,702 to 209,411,118 (741,417 blocks of 4096
bytes) directly behind the SiliconForest partition `p3` (192,633,862 to 208,669,701).
"Coordinator" steps are run from the workstation; the others at the machine.

### 1. Before (coordinator, the M1 in Linux; read-only)

    cd integrations/puredarwin/m1-native-boot/install
    ./build.sh && python3 -I -B test_kit.py
    . ../../../../private/disk-root/sf-part.env
    M1="ssh -o HostKeyAlias=10.1.1.127 chrisk@10.1.1.211"
    $M1 'sudo python3 - read /dev/nvme0n1 /tmp/sf-gpt-before' < linux/sf-gpt-check.py
    $M1 'sudo tar -C /tmp -cf - sf-gpt-before.head.bin sf-gpt-before.tail.bin' | tar -C ../../../../private/disk-root/gpt-backup -xf -
    python3 linux/sf-gpt-check.py check ../../../../private/disk-root/gpt-backup/sf-gpt-before \
        --partuuid $SF_PARTUUID --first $SF_PART_LBA --blocks $SF_PART_BLOCKS
    $M1 'grep -n "nvme0n1p\|/dev/disk/by-partlabel" /etc/fstab /etc/m1n1.conf /boot/loader/entries/* /etc/default/grub 2>/dev/null; cat /etc/fstab'

Expect `GPT ALL OK`, and no line of the Linux install that names a partition by number.
A current backup of macOS and of the Linux home exists (the owner's word).
Back out: nothing was changed.

### 2. macOS (owner starts it; coordinator copies the kit)

Owner: shut Linux down, hold the power button until "Loading startup options", choose
macOS, log in **at the screen** (the installer refuses when nobody is logged in there).
Coordinator, over ssh:

    scp out/siliconforest-mac-kit.tar <mac>:   &&   ssh <mac> 'tar -xf siliconforest-mac-kit.tar'

Back out: delete the directory `mac-kit`.

### 3. Look before writing (coordinator over ssh, or the owner; read-only)

    cd mac-kit && ./sf-install.sh --python-only        # fetches the Python bundle; prints "python ok"
    ./sf-preflight.sh 208669702 741417

Expect: `PRE ok   the kit's files are what was built`; the partition list with
`<- OURS` on a free space of 3,036,844,032 bytes after the SiliconForest partition
(macOS shows that partition as a Linux Filesystem with no name); `PRE ok` for "is there
exactly" and "large enough"; and either "the only free space the installer will offer" or
a NOTE naming the one to choose. **Any `PRE FAIL`: stop here.**
Back out: nothing was changed (the Python bundle is a directory inside `mac-kit`).

### 4. The installer (owner, in Terminal on the machine; this step writes)

    cd mac-kit && ./sf-install.sh

| It shows | Answer |
|---|---|
| sudo's password prompt | the macOS password |
| "Welcome to the SiliconForest installer" … "Press enter to continue." | Enter |
| the list of partitions and systems, then "Choose what to do:" with `f: Install an OS into free space` and `q: Quit` | `f` (if `f` is not offered: `q`, and stop) |
| "Choose an OS to install:" `1: SiliconForest` | `1` |
| only if more than one free area is large enough: "Choose a free area to install into:" | the one of 3.04 GB that step 3 named |
| "Enter a name for your OS" `OS name (SiliconForest):` | Enter |
| "Using macOS 13.5 for OS firmware", then downloads and "Preparing target volumes", "Adding partition SFBOOT (524.29 MB)", "Formatting as FAT", "Copying from esp into … partition" | wait |
| "Password for <user>:" then "Setting the new OS as the default boot volume..." | the macOS password |
| "Installation successful!" with `APFS VGID` and **`EFI PARTUUID`** | note the PARTUUID (it is SFBOOT's; put it into `private/disk-root/`, never into git) |
| instructions for the next step, "Press enter to shut down the system." | read step 5 first, then Enter |

After this: the two partitions exist, `sf/boot.bin` is on SFBOOT, the stub has its recovery
and our stage 1 waiting inside `Finish Installation.app`; **the default startup volume is
SiliconForest, which cannot start yet.**
Back out: if it stops before "Installation successful": run it again; with an incomplete
stub it offers `p: Repair an incomplete installation`; or remove everything ("Uninstall").
If `diskutil` refused to add a partition, nothing was changed.

### 5. Check from Linux before recovery (coordinator; read-only, plus one bench boot)

Owner: after the shutdown, **hold** the power button, choose the Linux entry (plain click:
once). Coordinator:

    $M1 'sudo python3 - read /dev/nvme0n1 /tmp/sf-gpt-after' < linux/sf-gpt-check.py
    $M1 'sudo tar -C /tmp -cf - sf-gpt-after.head.bin sf-gpt-after.tail.bin' | tar -C ../../../../private/disk-root/gpt-backup -xf -
    python3 linux/sf-gpt-check.py check ../../../../private/disk-root/gpt-backup/sf-gpt-after \
        --partuuid $SF_PARTUUID --first $SF_PART_LBA --blocks $SF_PART_BLOCKS \
        --free-first 208669702 --free-last 209411118 --before ../../../../private/disk-root/gpt-backup/sf-gpt-before --new 2
    $M1 'sudo ./sf-bootbin-install.sh <EFI PARTUUID> --show'     # read-only mount: sf/boot.bin and its sha256 (= out/boot.bin)
    $M1 'findmnt -rn | grep nvme; systemctl --failed --no-legend'  # Linux mounted what it always mounts

Expect `GPT ALL OK`: both copies valid and identical, our entry unchanged, every earlier
entry still there byte for byte, exactly two new entries inside the free space (APFS,
610,304 blocks; EFI-typed, about 128,000 blocks). The line "N earlier entries changed their
position" may be non-zero; Linux mounts by UUID. Then a new range list (the successor of
`runs/m1-storage/linux-d/ranges`: the free-space ranges now lie in the two new partitions,
which Linux never mounts) and **one disk-root boot from the bench** (the command of boot dr31
below): `NV_PART verified … found in both GPT copies` is the driver's own answer to "does it
still find p3".
If the check fails: do not go on to step 6. Nothing of SiliconForest starts from the disk
(the driver refuses by itself); the saved tables are the reference; what to restore is decided then.

### 6. Recovery (owner, at the machine; this step sets the boot policy)

Shut down. Wait 25 seconds. Press **and hold** the power button until "Loading startup
options". Choose **SiliconForest**. A "macOS Recovery" dialog may ask for a user and password
(if it asks to "select a volume to recover": the normal macOS volume). A Terminal opens:

| It shows | Answer |
|---|---|
| "SiliconForest installer (second step)", VGID, System volume; the explanation; "Press enter to continue." | Enter |
| `bputil`: a warning about lowering this volume's security, user name and password, a confirmation | the macOS user, password, `y` |
| `kmutil`: the same | the same |
| "Installation complete! Press enter to reboot." | Enter |

If it says "did not boot into the correct recoveryOS" or "not in One True RecoveryOS mode":
follow its text (it shuts down; hold the button in one continuous press, choose SiliconForest again).
Nothing is half-applied by a wrong password: it asks again.
Back out after this step: "Uninstall".

### 7. The first start (owner watches)

The restart starts the default volume, which is now SiliconForest: **the mark on black**
(stage 1, then stage 2, about 4 s), it stays while the kernel starts, then about 10 s later
the desktop in the look chosen earlier, with two terminals; Wi-Fi about 15 s after that.
If instead the log and "SiliconForest: stopped" appear, or the mark stays for a minute:
hold the power button until the machine is off; go to step 8 (the picker); see the table above.

To leave: menu **Session > Restart** (about 10 s), and **hold the power button** as the
screen goes dark, for the picker. (A plain restart would start SiliconForest again until step 8.)

### 8. Linux as the default again (owner)

In the picker: select the Linux entry, **hold Option** and click "Always Use". Check: restart
Linux once without touching anything; it comes back to Linux. From then on: SiliconForest =
hold the power button at start and choose it; every restart of SiliconForest lands in Linux.

### 9. Afterwards (coordinator)

The checks of step 5 again (table, mounts), `journalctl -k -b | grep "macsmc-reboot: PMU"`
(no boot-error line after a clean SiliconForest restart), `sudo e2fsck -fn` of `p3` through
`m1-storage/tools/dr_linux_root.sh wcheck` (clean, journal start 0).

## Updating the kernel later (no recovery mode, no macOS)

    ./build.sh                                              # workstation: out/boot.bin
    scp out/boot.bin linux/sf-bootbin-install.sh chrisk@10.1.1.211:
    ssh … 'sudo ./sf-bootbin-install.sh <PARTUUID of SFBOOT> boot.bin'

The script refuses any partition that is not FAT, labelled SFBOOT, EFI-typed, at most
600 MiB, unmounted, absent from `/etc/fstab` and not the EFI partition Linux booted from; it
refuses a file that is not stage + configuration + kernel with good checksums, built for the
root on this disk, in the installed variant, at most 60 MiB. It keeps the previous file as
`sf/boot.bin.old`; `--rollback` swaps them back; `--show` lists what is there. Stage 1 never
changes. **Prove a new `boot.bin` on the bench first** (`DR_BOOTBIN=`, below).
The script has not been run: there is no SFBOOT yet.

## Uninstall

1. Make another system the default first (step 8).
2. In macOS: `./sf-uninstall.sh` prints what it found and the commands;
   `sudo ./sf-uninstall.sh --execute` runs them after the container's name is typed:
   `diskutil apfs deleteContainer <the SiliconForest container>` and
   `diskutil eraseVolume free free <SFBOOT>`. If the first leaves a partition where the
   container was, the same `eraseVolume free free` on it.
3. From Linux: `sf-gpt-check.py check … --before <the read of step 1> --new 0` must say
   `GPT ALL OK`: the table is what it was before step 4 (entry positions aside).

The space is unallocated again; it is not given back to `p3` (that would move the write
fence a third time). The entry's boot policy stays behind, unreferenced, in `iSCPreboot`
(the Asahi project documents removal as deleting the partitions and nothing more).
**Not rehearsed:** the commands are from `diskutil`'s manual and the Asahi project's
documentation, and the script's way of finding the container has never met a real disk.

## What is proven, and what is first proven at install

`docs/roadmap/issues/SF-R-storage.md`, "Step 6", has the evidence lines. In short:

- **Proven on the M1 from RAM.** dr31: the bytes of `boot-bench.bin` (= `boot.bin` plus
  `sfsession_seconds=150 sfmark=1` in its configuration record) uploaded and entered, nothing
  else sent: desktop from the disk root 11 s later, Wi-Fi, clean self-restart with
  `pdnvme_ttl=0`, no controller reset, Linux finds the root clean and everything else unchanged.
  dr32: stage 1 as the installer writes it, with a PARTUUID nobody has: `Chainload failed:
  PartitionNotFound`, `SiliconForest: stopped`, no proxy, no kernel, disk unchanged.
  dr34: the same stage 1 with `chainload=<Linux's EFI partition>;m1n1/boot.bin` (read-only)
  finds the partition, reads the file, copies it to its own base and jumps: Linux came up by
  itself, its file and every watched range unchanged.
- **Proven on the workstation.** `test_kit.py`: the installer's own code with our data asks
  for exactly one EFI-typed 500 MiB partition after the stub, puts `sf/boot.bin` on it and
  builds the boot object from our stage 1 and the two variable lines. `sf-gpt-check.py`
  against the real table copies and altered ones.
- **First proven at install.** Stage 1 chainloading *our* `sf/boot.bin` from *our* FAT
  partition (dr34 did it with Linux's file from Linux's partition); iBoot starting our stage 1; picker name and icon; the installer's
  run on macOS and `diskutil`'s placement; the three macOS scripts and the Linux update
  script (never run); a start without the bench's cable; a session longer than 150 s.
  If any of the first three fails, the machine shows the stop message or Apple's recovery
  text and the way out is the picker; Linux and macOS are not involved.

Bench commands (the M1 in Linux, `private/disk-root/sf-part.env` present):

    BENCH_WHO=disk DR_RW=1 DR_HALT=reboot DR_STAGE=disk DR_PLAIN=1 DR_SECONDS=150 DR_BOOTBIN=$PWD/integrations/puredarwin/m1-native-boot/install/out/boot-bench.bin \
        tools/bench.sh run integrations/puredarwin/m1-storage/tools/dr_desk_slot.sh drNN native 300
    BENCH_WHO=disk DR_RW=1 DR_HALT=reboot DR_STAGE=disk DR_STAGE1=$PWD/integrations/puredarwin/m1-native-boot/install/out/sf-stage1-test.bin \
        tools/bench.sh run integrations/puredarwin/m1-storage/tools/dr_desk_slot.sh drNN native 180
