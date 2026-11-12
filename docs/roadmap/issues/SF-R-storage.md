## Hand-over: disk-root stream (12 November 2026; read this first)
The SiliconForest partition exists (64 GiB ext4, Linux `nvme0n1p3`, first block 192,633,862, 16,777,216 blocks of 4096 bytes) and holds the default desktop root. Its unique GUID and the filesystem UUID are in `private/disk-root/sf-part.env` (never in git; re-read with `sudo blkid` on the M1's Linux).

| Step | State |
|---|---|
| 1 Read-only driver at the tip, our partition visible | **Proven on the M1: boots dr01, dr02** (below) |
| 2 Root on the partition, read-only | **Proven on the M1**: shell (dr05) and the desktop (dr06) under the hypervisor, the desktop natively (dr08); see "Step 2". Open: a session started by launchd from the disk root; the timeline without the upload (step 4) |
| 3 Write-fenced driver | not started. Gate code and host-test list go to the reviewer BEFORE the first fenced-write boot |
| 4 Boot chain (B1 stage without the RAM root record) | not started |

Commands (all from the lab root; the bench is taken by `tools/bench.sh`, `BENCH_WHO=disk`, one boot at a time, names `drNN`):
- Host tests (run by every boot wrapper): `integrations/puredarwin/m1-storage/host-test/run-host-tests.sh` (`GATEHOST`, `PARTHOST` 48 cases, `SRCHOST`).
- One slot with the read-only driver and our partition (Linux baseline, boot, Linux again, verdict):
  `BENCH_WHO=disk NV_VERDICT_MODE=part NV_RANGE_FILE=runs/m1-storage/linux-b/ranges NV_TTL=300 NV_RUNTIME=420 tools/bench.sh run integrations/puredarwin/m1-storage/tools/nv_slot.sh drNN 3 any`
  (`linux-b/ranges`: 34 ranges for the layout with our partition; `runs/m1-storage/ref-tree-sw01.txt`: digest of the source image's tree, made with `user/pdnvme-test.c` built on the host over `debugfs rdump` of `artifacts/m1-desktop-010/pcie-enum-candidate-sw01/ramdisk.img`).
- QEMU rehearsal, no M1: `tools/dr_qemu_disk.py` (a 4096-byte-sector GPT disk around an ext4 image), `tools/dr_qemu.sh <name> <guest script> <disk> ro|rw` (the `qemu-dev` tools unchanged). Rehearse every new guest script there first: three script mistakes that would have cost M1 boots were found this way.
- A verdict failure writes `runs/m1-storage/STOP`; it is set aside only on the reviewer's word (dr01's is kept as `dr01-STOP-checker-and-node.txt`).

Traps: the collection has about 0x2000 bytes of room before the kernel image and members are padded to 16 KiB per segment (the kext is built `-Os` for that reason; `kc_overlap.py` fails the build otherwise); at the tip devfs is not ready until the root is mounted (about 75 s without `pd_quiet_wait=0 pd_rtc_wait=0`); `mount` resolves `/tmp/x` to `/private/tmp/x`; the shell exits on a failed redirection of `:`; `rm` asks on a read-only filesystem unless `-f </dev/null`; `qd_run.py`'s screenshot after `--up` froze the guest's serial output (the wrapper puts `--up` at the end); `pdnvme-test wtest` must never be pointed at a node that could be writable (it writes block 0 back).

### Step 1: the read-only driver at the tip with our partition visible (dr01, dr02)
Kext changes: `kernel/pd_nvme_part.c/.h` (plain C, host-tested): the partition is named by three boot arguments, `pdnvme_part=<unique GUID> pdnvme_part_lba=<first block> pdnvme_part_blocks=<length>`, and the driver reads both GPT copies itself (not through the storage stack): each header valid by its checksum and pointing at the other, both tables valid and byte-identical, exactly one entry with that GUID, of type Linux filesystem data, with exactly that first and last block, inside the usable range, overlapped by no other entry. Only then does `PDAppleNVMeMediaGuard` decline that one media (whose position, length and `UUID` property must also match); any doubt and it is guarded like the others. `kern.pd_nvme.part` says which; `kern.pd_nvme.media` gains `ours=N`. Nothing about writing changed: the media is write-protected, the gate is the same. Also: `/dev/pdnvme0` is retried once a second until devfs exists and no longer delays the block device (dr01's verdict failed on its absence at the tip; the rest of dr01 passed), and the kext is built `-Os`.
Host: `PARTHOST` 48 cases (every field, every boundary, either copy altered with checksums made right, duplicates, overlaps, unreadable blocks); the M1's real GPT copies (read from Linux, kept under `runs/`) are accepted with the real GUID and refused with the start or length off by one.
Collection (tip, default + PDAppleNVMe): `/nix/store/3qlsc4jqp9ng8vjnw4xl930nsi241km2-puredarwin-kc-arm64-0.1` (dr02); dr01 ran `dp558sl7…`.

Result of dr02 (hypervisor, RAM root, 8 CPUs, `pdnvme=3`, verdict PASSED 45 of 45; dr01 the same except the private node):
- our partition `verified … found in both GPT copies`; `partitions=8 guarded=7 foreign=0 ours=1` before and after; the stack's GPT equals Linux's 8 starts and sizes; Identify equals Linux's.
- `/dev/disk0s3` mounted `-o rdonly` with the tree's ext4 driver, twice: the whole tree (9,230 files, 764 directories, 106 links, 258,444,027 bytes) hashes to the source image's digest both times, 5.0 s each (about 52 MB/s through ext4; raw sequential 735 MB/s); three named files equal.
- writes: a mount without `rdonly` is refused (`Permission denied`), as is a remount read-write; create, mkdir, unlink and append in the mounted filesystem give `Read-only file system`; opens for writing are refused on `rdisk0`, `disk0`, `rdisk0s1`, `rdisk0s3`, `disk0s3`, `pdnvme0`. **Correction (found in the step-2 rehearsal):** the ext4 driver does try to write on a read-only mount (the console has `disk0s3: privilege violation.` six times in dr02: the superblock at mount and the journal superblock and superblock at unmount, for each of the two mounts); its own statistics (`writes=0`) count only writes that succeeded. Every one of them is refused by IOMedia, which was opened for reading only, before it reaches the driver (`write_refused` is 2, the two deliberate opens of `/dev/pdnvme0`), and the partition's blocks are unchanged (ranges, Linux `e2fsck`); `gate_rejected=0`; SMART inside Darwin: write counters +0.
- ranges: 24 of 24 on the GPTs, `p1`, `p2`, **`p3` (ours)**, `p4`, `p8` and the gaps identical in Linux before, Darwin (private node and block device) and Linux after; 6 of 6 untouched ranges in Linux's filesystems equal; 4 rewritten by Linux itself. The same digest over all ranges after the mounts as before.
- stop by the script after 22 s, clean; Linux: no recovery or replay line, `p5`/`p6`/`p7` mounted as usual, `p3` not mounted, `unsafe_shutdowns` 107 → 107.
- for step 3: a read-WRITE mount sets the journal's needs-recovery flag in the superblock at mount (`ext4_jnl_mount`) and clears it at a clean unmount; a root that is still mounted read-write when the machine is reset will therefore be replayed at the next mount.

### Step 2: root on the partition, read-only (dr03 to dr09)
How it is put together:
- **Root selection:** `rd=uuid boot-uuid=<FILESYSTEM UUID> wdt=-1`. The M1 kernel waits for the `boot-uuid-media` resource, which the tree's `Ext4FileSystemDriver` publishes for the Linux-typed leaf media whose superblock carries that UUID; disk numbers play no part. (QEMU's loader instead hands the kernel a match on the partition GUID: dr03 used that on the M1, found no root, and ended by the driver's time to live, cleanly.) The launcher's built-in `rd=md0` is replaced when the extra arguments carry an `rd=` (`launcher-diskroot.patch`, generated against the launcher base tree; `dr_desk_boot.sh` concatenates it after `m1-native-boot/launcher-native.patch`).
- **The RAM root stays uploaded**: it is `/dev/md0` (writable, memory), it carries each boot's script and test tools, and the same boot can be repeated with `rd=md0`.
- **Process 1** is `user/sf-roinit.c`, installed as `/usr/appleinternal/sbin/launchd.sfro` and chosen by `launchdsuffix=sfro` (DEVELOPMENT kernels; falls back to `/sbin/launchd` where the file is missing): devfs, the memory disk on `/private/ram`, then `execv` of launchd.
- **The root as changed for read-only use** (`tools/sf-root-transform.sh`; applied to the partition from Linux by `tools/dr_linux_root.sh transform <sf-roinit>`, which identifies the partition three ways and touches nothing else; `e2fsck -fn` clean afterwards): `/var`, `/private/var`, `/private/tmp`, `/etc/resolv.conf` are links into `/private/ram` (originals kept as `*.disk`); `/usr/bin/darwin-desktop-test`, which the bench types, runs the boot's own script from the memory disk (original kept as `.staged`). Reference digest of the transformed tree: `runs/m1-storage/ref-tree-diskroot.txt`.
- **No read of an owner's partition** (reviewer's condition): guarded partitions lose their content hint when the guard attaches, and with `pdnvme_readfence=1` the block device serves only the GPT's own blocks and, once identified, our partition; any other read is refused before a command is built. `kern.pd_nvme.fence` counts requests by where they lie. Range hashes go through `/dev/pdnvme0`, which the fence does not concern. **The fence is on in every boot from here.**
- **The end of a boot:** nothing can be read from the root once the controller stops, so `pdnvme-test stopreport` (one process, in memory, its report rehearsed once before the stop) stops the controller, reports and prints the bench's end marker. The driver's time to live remains the backstop.
- Commands: shell profile `BENCH_WHO=disk NV_VERDICT_MODE=root NV_RANGE_FILE=runs/m1-storage/linux-b/ranges NV_TTL=240 NV_RUNTIME=420 tools/bench.sh run integrations/puredarwin/m1-storage/tools/nv_slot.sh drNN 3 any`; desktop `BENCH_WHO=disk tools/bench.sh run integrations/puredarwin/m1-storage/tools/dr_desk_slot.sh drNN hv|native 240` (collection `build-kc-native.nix` = the native collection + PDAppleNVMe, `/nix/store/lpa9g137…` when dr06 to dr09 ran; `DR_BOOTARGS=pdnvme_delay=0` for the timeline). QEMU: `DR_QEMU_SINGLE=1 DR_QEMU_KERNEL=debug DR_QEMU_BOOTARGS="rd=uuid boot-uuid=<partition GUID in capitals> launchdsuffix=sfro wdt=-1 -s" DR_QEMU_DISK2=<state image> tools/dr_qemu.sh <name> guest/qemu-root-ro.sh <disk> ro`.

Results (every verdict 41 of 41 unless said):
| Boot | What | Result |
|---|---|---|
| dr03 | first attempt, `boot-uuid` = partition GUID | no root found; controller stopped cleanly by its time to live at 240 s; Linux and all stable ranges unchanged; STOP set aside on the reviewer's word |
| dr04 | shell, hypervisor | rooted on `disk0s3` read-only; 39 ok, 2 failures of the tree comparison: the `libSystem.B.dylib` in use reads back with 4 bytes changed in the page cache (`0x1f` to `0xff` at four offsets; the same digest in QEMU on a writable root). Comparison corrected (that file's content is compared separately) |
| dr05 | shell, hypervisor | **passed**: root read-only on our partition, memory disk on `/private/ram`, writes to `/var`, `/tmp`, `resolv.conf` succeed, creates in `/etc`, `/usr`, `/`, `/private` and a remount read-write refused; the whole root tree as the running system sees it (9,232 files) equals the transformed source image; fence: before the deliberate test `refused=0 other=0`, the deliberate read of `p2` through `/dev/rdisk0` refused (`Operation not permitted`); about 12,000 requests, 67,366 blocks, all inside our partition; one superblock write tried by ext4 at the root mount and refused by IOMedia; `write_refused=0 gate_rejected=0`, SMART write counters +0; stop from memory, report complete; Linux clean, 24 of 24 stable ranges unchanged |
| dr06 | desktop, hypervisor | **passed**, and the desktop from the disk: GL on KMS 2560x1600, 10 flips, 0 missed, Wi-Fi join and ping, Touch Bar ready, coreaudiod (`hold_probe=1`), keyboard and trackpad, both terminals, clean end |
| dr07 | desktop, native | storage verdict passed; the compositor came up on the software renderer: driver log lines on the 115200-baud UART delayed the display driver (the native stream's `noiolog` word was missing) |
| dr08 | desktop, native, `noiolog` | **passed**: GL on KMS, 10 flips, 0 missed, Wi-Fi, Touch Bar, coreaudiod, input, terminals, no hypervisor |
| dr09 | the same with `pdnvme_delay=0` | **passed** (41 of 41, GL on KMS): the driver's 3 s wait before starting the controller is not needed |

Seconds from the kernel's start (`m1-native-boot/tools/nb_times.py`; typed sessions; the driver waited its default 3 s before starting the controller in these boots): dr06 (hypervisor) launchd 4.97, shell 5.38, script 8.38, first frame 10.18; dr08 (native) launchd 5.17, shell 5.38, script 10.56, first frame 12.82. dr09 (native, `pdnvme_delay=0`): launchd 2.04, shell 2.35, prompt 4.95, command typed 6.11, script 7.33, first frame 9.69, Wi-Fi joined 12.60 (the 5 s between the shell and the script are the bench typing at a 115200-baud console; a launchd-started session does not have them). RAM-root references (SF-T): first frame 7.27 (hypervisor), 8.67 (native, typed), 4.91 (native, launchd session). The RAM root is still uploaded in these boots, so the saving of the upload shows only in step 4.

Known and open: the ext4 driver writes the superblock at every mount, read-only or not (refused here by IOMedia): step 3 must either make a read-only mount truly read-only or have those writes inside the fence and correct. The in-use libSystem's four patched bytes are in the cache: step 3 must show they are not written back to a writable root. A session started by launchd (multi-user) from the disk root is not tried.

## Goal
SiliconForest reads, and later boots from, the M1's internal NVMe storage instead of a RAM disk uploaded over USB. The internal disk holds the owner's macOS and Linux installs: **nothing in this ticket may modify it** until the owner authorises a specific write, and the code is built so that it cannot.

## Status (2026-11-11)
| Phase | State |
|---|---|
| 1 Facts | Sources, saved Apple device tree, hypervisor: done. Controller state as left by the boot chain: observed on the M1 (boot nv01, nothing written). Linux' view (read-only): partition layout, Identify, SMART baseline, kernel log, and reference SHA-256 of 30 ranges (1 MiB each: start incl. GPT, seven points across the disk, start/middle/end of every partition, end incl. backup GPT) in `runs/m1-storage/linux-a/`. |
| 2 Read-only driver `PDAppleNVMe` | **Proven on the M1** at level 2 (boot nv02) and level 3 (boot nv03): controller start, Identify equal to Linux', all comparable ranges equal through the driver's own device and through the block device, GPT parsed by the stack, every partition guarded, no write, clean stop (twice in nv03), Linux clean afterwards. **No further boots** until the reviewer says so. |
| 3 Filesystem groundwork | Source survey done; ext4 read-write on a 4096-byte-sector GPT disk and the behaviour of write-protected media shown in QEMU. |
| 4 Install design | Written below. Nothing of it is implemented or executed. |

Proven on the M1: what nv01 observed and what nv02 measured ("Result of nv02"). The block-device path is compile evidence.

## Phase 1: facts

### Hardware map (T8103 / J293)
From the Omacom tree (`arch/arm64/boot/dts/apple/t8103.dtsi`, `t8103-pmgr.dtsi`, `drivers/nvme/host/apple.c`, `drivers/soc/apple/sart.c`, `rtkit.c`), m1n1 `src/nvme.c`, and this machine's Apple device tree as saved by an earlier boot (`runs/m1-bringup-009/observed-original-adt.bin`; both agree):

| Item | Value |
|---|---|
| Coprocessor (ASC) registers | `/arm-io/ans` reg 0: `0x2_7740_0000`, CPU control at `+0x44` (RUN = bit 4) |
| Mailbox | ASC `+0x8000` = `0x2_7740_8000` (Linux `ans_mbox`, asc-mailbox-v4); AIC 583–586 |
| NVMe + linear SQ + NVMMU registers | `/arm-io/ans` reg 3: `0x2_7BCC_0000` (Linux maps 0x40000; ADT says 0x60000); AIC 590 |
| SART | `/arm-io/sart-ans`: `0x2_7BC5_0000`, version 2, 16 entries (`CONFIG` at `4*i`, `PADDR` at `0x40+4*i`, 4 KiB units, allow flags `0xff`) |
| Power | ADT `clock-gates = [103]` = `APCIE_ST` (PMGR `0x2_3B70_0418`), parents `ANS2` (`0x2_3B70_03F0`) and `APCIE` (`0x2_3B70_0348`, shared with Wi-Fi). Linux: `power-domains = <&ps_ans2>, <&ps_apcie_st>`, `resets = <&ps_ans2>` |
| RTKit | system endpoints only (management, crashlog, syslog, debug, ioreport, oslog); no application endpoint is used for NVMe. Shared buffers the firmware requests are host memory, allowed in the SART, named by physical address |
| DMA | no DART. Data and queue memory are physical addresses; each command needs a TCB in the NVMMU table (indexed by tag) carrying the same PRPs and a direction flag |
| Queues | one admin queue (Linux depth 2, tag 0) and one I/O queue (depth 64); tags are shared between them and limited to 64; "linear" submission: the command is written to slot `tag` and the tag to a doorbell (`0x2490c` admin, `0x24910` I/O); completion queues are ordinary |
| Register writes at bring-up | `LINEAR_SQ_CTRL=1`, `MAX_PEND_CMDS_CTRL=64|64<<16`, `NVMMU_NUM_TCBS=63`, `AQA`, `ASQ`, `ACQ`, NVMMU TCB bases, `CC` |
| Namespaces | ADT `namespaces` lists ids 1, 2, 3; Linux shows `nvme0n1` (the disk), `n2`, `n3` (small, firmware-owned). Only namespace 1 is read by this driver |
| LBA size | 4096, no metadata (Linux' Identify Namespace: `lbads=12`, `nsze=ncap=nuse=244276265`); the driver refuses anything else |
| Identify Controller as Linux reads it | `vid=0x106b`, model `APPLE SSD AP1024Q`, firmware `590`, `mdts=8` (1 MiB per command: the driver's limit of 256 blocks is exactly that), `nn=3`, `oacs=0x4`, `oncs=0x4`, `vwc=1`, `aerl=1`, `sqes=0x76`, `cqes=0x44`, `rtd3e=0` (shutdown timeout 5 s); Linux runs it with 2 queues, depth 64. Full field list for the comparison with Darwin: `runs/m1-storage/linux-a/nvme-id.txt` |
| Partition table as Linux sees it (read live, `lsblk`/sysfs, read-only) | 1 TB (244,276,265 blocks of 4096 bytes), GPT, 7 partitions: `p1` Apple Silicon boot (APFS, 500 MiB, starts at block 6), `p2` macOS APFS (~798 GiB), `p3` APFS (2.3 GiB, the Linux stub), `p4` EFI System (vfat, 500 MiB, mounted `/boot/efi`), `p5` ext4 2 GiB `/boot`, `p6` btrfs ~123 GiB (`/`, `/home`, …), `p7` Apple Silicon recovery (5 GiB, last). `nvme0n2` 3 MiB and `nvme0n3` 128 MiB are the firmware's namespaces. Linux' request limit: 1024 KiB per command |

### What the boot chain leaves behind
iBoot → m1n1 stage 1 (in the Linux stub volume) → stage 2 `boot.bin` on the EFI partition (this lab's "boot window" build) → RAM-loaded m1n1 → hypervisor → XNU.
- Stage 1 uses the NVMe to read stage 2 and then runs m1n1 `nvme_shutdown()`: delete I/O queues, `CC.SHN` normal, `CC.EN=0`, RTKit sleep, PMGR reset of `ANS2`. Linux' comment says the same: "an ANS NVMe controller is handed off with firmware shut down, in an NVMe disabled state, after a clean shutdown".
- The RAM-loaded m1n1 does not initialise the NVMe (its `nvme_shutdown()` is a no-op when not initialised).
- **Observed (boot nv01, `pdnvme=1`, nothing written, 8 CPUs, M1 back in Linux):** `ANS2=0x0f0020ff`, `APCIE_ST=0x0f0000ff` (both on), `APCIE=0x000000ff`; `CPU_CONTROL=0` (coprocessor stopped); both mailbox directions empty; `CC=0x00474000` (`EN=0`, `SHN=01`: the normal-shutdown notification m1n1 left, I/O SQ entry size 7), `CSTS=0`, `AQA=0x003f003f`, boot status 0, linear SQ still enabled, `NVMMU_NUM=0x3f`, `CAP=0x20f0010040` (timeout field `0xf0`: 120 s), `VS=1.1.0`. This is the stopped, cleanly shut down state Linux expects to find. The driver's rule stands: a coprocessor found running is left alone.
- SART as left by the boot chain: entries 0–3 with flags `0xff` (three in the `0x2_3Dxx_xxxx`/`0x2_3Exx_xxxx` range, one of `0x34000` bytes near the top of RAM) and entry 5 with flags `0xea` (`0x1a0000` bytes at `0x8_0003_4000`). As in Linux, every entry found with flags is treated as the boot loader's and never written; 11 entries are free, the driver needs about 4.
- iBoot's view in the ADT (`iop-ans-nub`: `pre-loaded`, `running`) describes its own hand-off to m1n1, not what m1n1 leaves.

### The hypervisor
m1n1's hypervisor maps all MMIO straight through except what it hooks (`proxyclient/m1n1/hv/__init__.py`; the lab launcher adds no ANS, SART or NVMe hook):
- hooked: the virtual UART; PMGR registers of `UART0`, the hypervisor's USB port (`ATCx_USB`, `ATCx_USB_AON`) and their parents (`UART_P`, `SIO`, `SIO_BUSIF`, `ATCx_COMMON`); CPU start registers; two power-gate overrides.
- **not** hooked: `ANS2`, `APCIE_ST`, `APCIE` (PMGR), the ASC, the mailbox, the NVMe/NVMMU registers, the SART. Nothing of the storage path is trapped or emulated; DMA is physical (stage 2 maps guest RAM 1:1), so PRPs are real addresses.
- Consequence: under the hypervisor the controller sees exactly what the driver writes. Nothing is filtered for us, and nothing is added. The one hypervisor-specific risk is the end of a boot: the bench resets the M1 from outside (`macvdmtool reboot`), so a controller still enabled at that moment is cut off like a power loss (see "Shutdown").

### What bring-up changes on the device, compared with Linux
| Step | Linux (every boot) | m1n1 stage 1 (every boot) | PDAppleNVMe |
|---|---|---|---|
| PMGR reset of `ANS2`, coprocessor start, RTKit boot | yes | yes | yes (same order) |
| SART entries for RTKit buffers | yes | yes | yes; cleared at stop |
| `CC.EN=1` | yes | yes | yes |
| Identify Controller / Namespace | yes | no | yes (controller, namespace 1) |
| Create I/O CQ + SQ | yes | yes | yes |
| Set Features (number of queues `0x07`; plus whatever `nvme_init_ctrl_finish`/`nvme_start_ctrl` decide: timestamp, power state, async events) | yes | no | **no** |
| Get Log Page | yes (SMART through hwmon, command effects) | no | SMART only |
| Read | yes | yes | yes |
| Write / Flush / Dataset Management (the root is mounted `discard=async`) | yes | no | **no: not in the binary** |
| Delete queues, `CC.SHN` normal shutdown, `CC.EN=0`, RTKit AP quiesced + IOP sleep, `CPU_CONTROL=0` | yes (`apple_nvme_shutdown`) | yes | yes (same sequence) |

SMART, read from Linux (read-only passthrough) around ordinary bench cycles and around nv02: `power_cycles` grows by **1 per machine reset** (two per bench cycle: Linux' reboot and the external reset), whoever starts the controller afterwards; `unsafe_shutdowns` stays at 107 (the external reset of a stopped controller does not count); `host_write_commands` grows by roughly 600–1400 at each Linux shutdown and boot (Linux' own writes); `media_errors` 0, `percent_used` 4. Linux' kernel log of an ordinary boot shows no recovery, replay or orphan line; `p4` (EFI), `p5` (ext4 `/boot`) and `p6` (btrfs) are mounted read-write on every boot.

## Phase 2: the read-only driver

Directory `integrations/puredarwin/m1-storage/` (GPL-2.0 kext; port of the Linux driver's T8103 path, polled RTKit client taken from `m1-sound/aop`).

| File | What |
|---|---|
| `kernel/pd_nvme_gate.c/.h` | the allowlist: plain C, linked into the kext and into the host test |
| `kernel/pd_ans.cpp/.h` | SART, RTKit boot/shutdown, controller enable, Identify, queue pair, read, SMART, shutdown. One thread, polled, no interrupt handler |
| `kernel/PDAppleNVMe.cpp` | boot arguments, engine thread, `/dev/pdnvme0`, the `IOBlockStorageDevice`, sysctls |
| `host-test/` | gate test and source checks |
| `user/pdnvme-test.c` | guest tool: SHA-256 of LBA ranges, sequential and random read timing, write attempt |
| `guest/`, `tools/nv_boot.sh`, `tools/nv_summary.sh` | bench boots (`BENCH_WHO=storage`, names `nv01`…) |
| `tools/linux_m1.sh` | read-only fact gathering and reference hashes on the M1's Linux |
| `build-kext.nix`, `build-kc.nix`, `build-pdnvme-test.nix` | recipes; the collection is the current default (`m1-touchbar/build-kc.nix`) plus this kext |

Collection: `/nix/store/a3xw8ld5pqg4vvhdgcshlyqcarxn9k2c-puredarwin-kc-arm64-0.1`, 0 overlapping segments, room left after `__PRELINK_INFO` `0xba000` (default: `0xe3000`; the kext costs `0x29000`).

### Read-only enforcement
1. **No write path exists.** The driver has names for seven commands only: admin Delete SQ, Create SQ, Get Log Page, Delete CQ, Create CQ, Identify; I/O Read. There is no code that builds Write, Write Zeroes, Flush, Dataset Management, Format, Sanitize, Firmware, Namespace Management, Set Features, Security or vendor commands, and no build option that adds one.
2. **One doorbell, behind a gate.** `PDAns::ring()` is the only statement that writes a submission doorbell. Before it, `pd_nvme_gate()` checks the command twice: as built, and again as the 64 bytes sitting in the queue slot the controller will read. The gate admits per queue: admin = the six opcodes above with their exact fields (Identify CNS 0/1 only, log page 2 only, queue id 1 only); I/O = opcode `0x02` on namespace 1, at most 256 blocks, inside the namespace, no FUA or other flags. Anything else is refused, the slot is zeroed, and `kern.pd_nvme.gate_rejected` is incremented. Because admin `0x00`/`0x01` share numbers with I/O Flush/Write, the check is per queue and on fields, not on the opcode alone.
3. **Direction.** The NVMMU's host-to-device flag is set only as Linux sets it for Create I/O CQ/SQ (the "data" is the queue's address); an I/O command without the device-to-host flag is refused.
4. **Bounce buffers.** Reads land in eight 1 MiB buffers the driver owns and are copied out; device DMA can never target other kernel memory, and a timed-out command's buffer is never reused.
5. **Above the driver.** `reportWriteProtection` is always true, so IOBlockStorageDriver publishes media that is not writable and no filesystem can mount it read-write; `doAsyncReadWrite` refuses any buffer that is not device-to-host before building anything; format and unmap are refused; synchronize sends nothing. `/dev/pdnvme0` refuses opens for writing. These refusals count in `kern.pd_nvme.write_refused`.
6. **Off by default, staged.** Without `pdnvme=` nothing is touched (no power domain, register or allocation). `pdnvme=1` observes; `2` starts the controller and offers only `/dev/pdnvme0` (no IOMedia, so no partition scheme or filesystem driver sees the disk); `3` also publishes the block device.
7. **Proof by test.**
   - Host (`host-test/run-host-tests.sh`, run by `nv_boot.sh` before every boot): every opcode on both queues (exactly 7 pairs allowed), 34 named destructive commands in realistic shapes, read bounds, 20 million random entries compared with an independently written statement of the rule; source checks (one doorbell write, after both gate calls; only allowlisted opcode names; write protection constant). Result: `GATEHOST PASSED`, `SRCHOST PASSED`.
   - Machine (guest script): `gate_selftest` = 7 allowed / 505 refused / 0 wrong; `gate_rejected` = 0 at the end; opening `/dev/pdnvme0`, `/dev/rdiskN`, `/dev/diskN`, a partition for writing must fail (if an open were granted, the tool writes block 0 back with its own contents, so even a carried-out write would change nothing); hashes equal Linux' before and after.

### What else could touch the disk once it is published (`pdnvme=3`)
- The RAM root has no diskarbitrationd, autodiskmount, fsck, Spotlight, swap or automount; `/etc/fstab` is empty; the lab boots single-user, so no daemon runs. Nothing mounts anything by itself.
- In the kernel: the partition schemes read the GPT; `ApfsFileSystemDriver` (PureDarwin's own) attaches to APFS-typed partitions and **reads** the owner's macOS containers (it writes only through an explicit user-client call, which needs a writable open and cannot get one); `Ext4FileSystemDriver`/`AppleFileSystemDriver` stay idle with `rd=md0`. A parser bug there could panic the kernel on real macOS data, which would end the boot without a controller shutdown. That is why level 2 exists and comes first, and why the first level-3 boot is its own step.

### Shutdown
The bench ends every boot with an external reset. The controller must already be stopped then:
- the guest script ends with `kern.pd_nvme.stop=1` and waits for `stopped-clean` before `PD_BENCH_DONE`;
- independently the driver stops the controller itself `pdnvme_ttl` seconds after it became ready (the wrapper sets 120; a boot reaches the guest script about 12 s after the kernel starts, and the bench's reset comes at the runtime bound of 300 s only if the script never finishes);
- `systemWillShutdown` does the same.
Not covered: a kernel panic or hang before the stop. The controller is then reset while enabled, as when the power button is held under Linux; with no write in flight there is nothing to lose, but `unsafe_shutdowns` would grow by one. It is checked after every boot.

### Boot plan (each step only after the previous one is clean; after each: M1 back in Linux, Linux root mounted without recovery, SMART compared)
| Boot | Level | Purpose |
|---|---|---|
| nv01 | 1 | observe: PMGR states, `CPU_CONTROL`, `CC`/`CSTS`, SART entries as left by m1n1. Writes nothing; does not start the controller |
| nv02 | 2 | **first controller start (needs the go).** RTKit, Identify vs Linux, hashes of the ranges through `/dev/pdnvme0`, throughput, latency, write refusal, SMART start/end, clean stop |
| nv03 | 3 | done: block device with the partition guard, GPT through the stack, same hashes through `/dev/rdisk0`, write attempts through the block layer, nothing mounted, nothing attached, second start |
Before nv02 and after each: `tools/linux_m1.sh all` and `hash` (ranges: start, GPT, seven points across the disk, start/middle/end of every partition, end). Ranges Linux itself changes between its two boots (inside its mounted filesystems) are reported separately: for those, equality with Darwin cannot be attributed.

### Result of nv02 (`pdnvme=2`, 8 CPUs, runtime bound 300 s, `pdnvme_ttl=120`; collection `/nix/store/a3xw8ld5pqg4vvhdgcshlyqcarxn9k2c-puredarwin-kc-arm64-0.1`)
One bench slot, nothing else in between: Linux baseline → Darwin boot → Linux again → `tools/nv_verdict.py` (`runs/m1-storage/nv02-*`).
- **Bring-up:** found stopped as in nv01; RTKit protocol 12 in 142 ms; controller ready 150 ms after the driver started; firmware asked for three buffers (crashlog, ioreport, syslog), placed in SART entries 4, 6, 7 and cleared at stop.
- **Identify:** every field of Identify Controller and Identify Namespace 1 that the two sides print is equal to Linux' (model, firmware, `mdts`, `nn`, sizes, LBA format).
- **Hashes (30 ranges of 1 MiB):** the 20 ranges on the GPTs and the APFS partitions (`p1`, `p2`, `p3`, `p7`) are identical in Linux before, under Darwin, and in Linux after. Of the 10 ranges inside Linux' mounted filesystems, 6 were not touched by Linux between its two readings and are equal under Darwin too; 4 (`p4` start, `p5` start and end, one in `p6`) differ between Linux-before and Linux-after, i.e. Linux rewrote them at its shutdown or boot, and nothing can be attributed there.
- **Writes:** `gate_rejected` 0; `write_refused` 2 = the two deliberate opens for writing of `/dev/pdnvme0`, both refused with "Read-only file system"; SMART read inside Darwin at start and end: `host_write_commands` +0, `data_units_written` +0, `host_read_commands` +6591 (= the driver's own count of reads), `unsafe_shutdowns` +0, `power_cycles` +0. Commands sent in the whole session: Identify ×2, Create CQ, Create SQ, Get Log Page ×3, Delete SQ, Delete CQ, Read ×6591. No error, timeout, stray completion or NVMMU invalidation failure.
- **Speed (polled, every read copied once, at most 8 commands of 1 MiB in flight):** sequential 512 MiB at 8 MiB per call: **710 MB/s**; 1 MiB per call: 608 MB/s; 128 KiB per call: 282 MB/s. Random 4 KiB reads over the whole disk, one at a time: **98 µs average, 99 µs median, 235 µs 99th percentile**, 832 µs worst, about 10,000 per second. Linux on the same disk reaches several times the sequential figure; the copy and the single polling thread are the limit here, not the disk.
- **Stop:** by the guest script 14 s after it began (the time-to-live of 120 s was not needed): queues deleted, normal shutdown notification completed (the firmware logged `NVMe shutdown start … saveCtx`), `CC.EN=0`, RTKit AP quiesced / IOP sleep, coprocessor stopped, state `stopped-clean`. The capture ended there; the external reset came with the controller off.
- **Linux afterwards:** verified by the bench; kernel log without recovery, replay or error lines for NVMe, btrfs, ext4 or vfat; `p4`, `p5`, `p6` mounted read-write as usual; controller `live`; `unsafe_shutdowns` 107 → 107; `media_errors` 0.
- **Open observation:** at start the firmware's own log said `gc_search_bm.c: Restarting rebuild from lba …` and, 150 ms later, `Finished rebuilding GC Search BM`. Linux' kernel log (three boots checked, including the one after nv02) shows only `boot mode normal` from the firmware. It is firmware-internal (no host write was counted) and whether Linux simply does not get this message, or something in our start provokes the rebuild, is not known.
- **Checker mistake, for the record:** the first run of the verdict reported one failure and wrote the STOP file: its "driver anomaly" pattern matched the script's own `NV_WRITE_REFUSED` marker. No condition was off; the pattern was corrected and the verdict re-run on the same data (`NV_VERDICT PASSED`); the stale file is kept as `runs/m1-storage/nv02-STOP-checker-false-positive.txt`.

### The partition guard (`pdnvme=3`)
Partition schemes, the tree's APFS container scheme and any other content driver for IOMedia compete in one match category (`IOStorage`), one winner per media. The kext now carries `PDAppleNVMeMediaGuard`: it enters that category with the highest score for every media that is a partition below `PDAppleNVMeDisk`, wins, and does nothing (it never opens the media). Effect: the whole disk still goes to the GPT scheme (the GPT is parsed by the stack), every partition gets its `/dev` nodes for raw read-only access (IOMediaBSDClient is in another category), and **no APFS container, filesystem or other content driver attaches to the owner's partitions**. Media of other disks is declined. The filesystem kexts themselves only act on a `mount`, which nothing issues. A future SiliconForest partition would be exempted by its type GUID. Proven in nv03.

### Result of nv03 (`pdnvme=3`, partition guard, 8 CPUs, runtime bound 300 s, `pdnvme_ttl=120`; collection `/nix/store/w73qb4lhzb9m6nkvm0npp39b4r2dzf90-puredarwin-kc-arm64-0.1`)
Same slot procedure as nv02 (`runs/m1-storage/nv03-*`); the verdict passed on its first run.
- **Guard first:** before any read through the block layer the guest script required `published=1 partitions=7 guarded=7 foreign=0` from `kern.pd_nvme.media` (a walk of the registry below the block device: anything that is not IOBlockStorageDriver, IOMedia, IOGUIDPartitionScheme, IOMediaBSDClient or the guard counts as foreign) and no mount from a disk; the same line was read again after the tests. The console names the seven guarded partitions and has no line from the APFS or any filesystem driver. `mount` showed the RAM root and devfs only.
- **GPT through the stack:** `gpt -r show /dev/rdisk0` lists 7 partitions whose starts and sizes equal Linux' (4096-byte blocks), plus primary and backup headers and tables.
- **Reads through the block device:** the 30 ranges through `/dev/rdisk0` hash exactly as through `/dev/pdnvme0` in the same boot; against Linux the result is the same as in nv02 (20 of 20 on the GPTs and APFS partitions identical before, under Darwin and after; 6 of 6 untouched ranges in Linux' filesystems equal; 4 rewritten by Linux itself). Sequential 256 MiB through `/dev/rdisk0`: 765 MB/s; through `/dev/pdnvme0` 751 / 609 / 282 MB/s (8 MiB / 1 MiB / 128 KiB per call); random 4 KiB: 87 µs average, 94 µs median, 140 µs 99th percentile.
- **Write attempts through the block layer:** opening `/dev/rdisk0`, `/dev/disk0` and `/dev/rdisk0s1` for writing (read-write and write-only) fails with "Permission denied" in the storage family, above the driver; `/dev/pdnvme0` as before. `write_refused` = 2 (the two opens of `/dev/pdnvme0`; the block-layer attempts never reach the driver), `gate_rejected` = 0.
- **SMART, whole 512-byte page, compared byte by byte** around each start and stop inside Darwin: only the temperature, `data_units_read` and `host_read_commands` change; nothing in the write counters, `power_cycles`, `unsafe_shutdowns`, error fields or the vendor area. From Linux: `unsafe_shutdowns` 107 → 107.
- **Second start in the same boot** (stop cleanly, start again as Linux does on a driver rebind, hash all ranges, stop cleanly again): ready in 138 ms, same digest over all ranges, stopped cleanly.
- **Linux afterwards:** verified, no recovery/replay/error line, `p4`/`p5`/`p6` mounted as usual.

### The firmware's "GC Search BM" message
- nv02, first start: `Restarting rebuild from lba 121836800`, then `Finished rebuilding GC Search BM`. nv03, first start: the same with `lba 131446574`. nv03, second start in the same boot: **no rebuild message**, only `boot mode normal`.
- Linux prints every firmware log line (`dev_info`), and its journal of 49 boots has `boot mode normal` 49 times and never a `gc_search_bm` line. m1n1 prints such lines only on its own console; Linux' journal does not hold the boot loaders' output.
- Reading: the rebuild is something the firmware does once after a machine reset, in the first session that gets that far, and it is finished within about 150 ms. On the way to Linux that session is U-Boot's (it reads the kernel from the disk before Linux starts the controller again), so Linux never sees it; on our way there is no U-Boot, so we do. The second-start test supports this (not repeated within one power cycle). **Not shown directly:** U-Boot's own firmware log was not captured, and stage-1 m1n1 runs a short session before both paths without, apparently, completing it. The start sequence itself was compared with Linux' again (reset pulse, RTKit boot order, registers, Identify, queue creation): no difference found other than the commands we leave out.
- Also seen, at the second stop only: `Util_Reset_FailGeom Info` and `Comressing DM.version: 178` between the usual `NVMe shutdown start` and `saveCtx … took 171 ms`. The firmware saves its context at every clean shutdown (m1n1's, U-Boot's, Linux' and ours alike); what these two extra lines mean is not known, and Linux' journal ends before its own shutdown, so there is nothing to compare with. No SMART field moved.

### Not done in this driver
Next, host-side only until a new go: interrupts and zero-copy reads. Not done: interrupts (AIC 590; the engine polls, spinning only while a read is out), zero-copy DMA (everything is copied once), more than 8 reads in flight, namespaces 2 and 3, error recovery beyond "stop using the controller and shut it down", sleep.

## Phase 3: filesystem
Survey of the pinned PureDarwin tree (`e7aa00a6`) and the booted collection:
| Filesystem | In our collection | Read-write | Root on a real disk | Notes |
|---|---|---|---|---|
| ext4 (PureDarwin `ext4.kext`, JBD2 journal with replay) | yes | yes | **yes** | answers the kernel's root-authentication ioctl; every arm64 image of the tree roots on it; QEMU's `virt` image boots it from a GPT disk today. `mkfs`/`fsck` exist on the Linux side |
| APFS (PureDarwin's own `apfs.kext` on `libapfsrw`, MIT) | yes | yes, for 4096-byte-block, unencrypted, single-volume containers | possible | young; no `fsck`; macOS would see and might touch it |
| HFS+ (Apple sources, journaled) | yes | yes | **no**: returns `ENOTTY` for the root-authentication ioctl, the kernel panics (`bsd_init.c:934`); no `newfs_hfs`/`fsck_hfs` in the tree | |
| FAT (`msdosfs`) | yes | yes | no | for the EFI partition |
| exFAT, UDF, NTFS, ZFS, UFS, NFS client, `vn` file-backed disks | no | | | |
Partition schemes built: GPT, MBR, Apple Partition Map, plus the APFS container scheme. `IOStorageFamily-260.100.1` is in the collection. An open `IONVMEFamily` exists in the tree but is for PCI NVMe controllers; it cannot drive the ANS.

**Choice: ext4** in a GPT partition with 4096-byte blocks.

Root selection facts that matter for the design:
- today: `rd=md0`, a memory device (not IOMedia). Our whole disk therefore becomes `disk0`.
- without `rd=md*`, the root must come from `rd=diskNsM`, or a `root-matching` dictionary / `boot-uuid` in `/chosen` published by the loader.
- if the root device does not appear within 60 s, this kernel calls `IOSetRecoveryBoot` (**an NVRAM write**: `boot-command=recover-system`) and panics, unless `wdt=-1` is in the boot arguments. No NVRAM controller was found in our platform code, so the write probably goes nowhere, but that must be shown, or the path patched out, before any boot waits for a disk root.

Demonstration without the M1's disk (QEMU `virt` under TCG, the same arm64 kernel binaries, through the other stream's `qemu-dev` tools unmodified; files in `runs/m1-storage/qemu/`):
- the system itself roots on ext4 from a GPT virtio disk, read-write (`root_device on / (ext4, local, root file system)`);
- a second disk with **4096-byte sectors**, a GPT written for that sector size and one ext4 partition (4096-byte blocks, made on the host): the GPT is parsed (`256 60000 1 GPT part - Linux data`), the partition mounts read-write, a text file and a 20 MiB file are written, unmount, remount, both read back; on the host afterwards `e2fsck -fn` is clean and the text file is there;
- the same image attached **read-only** (the stack sees write-protected media, as it will with PDAppleNVMe): a read-write mount fails with "Permission denied", `-o rdonly` mounts and lists, writes to `/dev/rdiskN` and `/dev/diskNs1` fail with "Permission denied", and the image's SHA-256 is unchanged afterwards.
- two things seen on the way: (1) **`diskN` numbers are not stable**: the system disk was `disk0` in some boots and `disk1` in others, so a root must be named by partition UUID (`boot-uuid`/`root-matching`), never `rd=disk0sN`; (2) the 20 MiB write took 100 s (about 0.2 MB/s) under TCG. Whether that is the emulator or the ext4 driver's write path is not known and must be measured on real hardware before ext4 is called fast.
- not shown: a root on the 4096-byte-sector disk (the root in these runs is the 512-byte system disk); journal replay after an unclean stop; anything about APFS.

## Phase 4: design for the real install (nothing here is executed)

### Partition
A dedicated GPT partition, 32–64 GiB, ext4, with **its own type GUID** (not the generic Linux one, so that no rule can ever confuse it with the Linux root) and the name `SiliconForest`.
| Option | Who, from where, with what | Risk | Undo |
|---|---|---|---|
| P1 (recommended) shrink macOS' APFS container | the owner, in macOS: `diskutil apfs resizeContainer` (what the Asahi installer does), then add the partition in the free space | APFS shrinks online and is the vendor's supported path; needs free space in macOS | delete the partition, grow the container back |
| P2 shrink the Linux btrfs root | the owner, in Linux: `btrfs filesystem resize`, then shrink `p6` and add a partition (`parted`/`sfdisk`, 4096-byte sectors) | two manual steps that must agree; a wrong order or size destroys the Linux root; the recovery partition must stay last | delete, grow back; **back up first** |
| P3 no partition: an image file | — | our kernel has no file-backed disk driver and no btrfs; not viable | |
`mkfs.ext4 -b 4096` on the new partition is run from Linux (a write to the new partition only). Every step above is a write to the internal disk and needs the owner's explicit go; a mistake in P1/P2 is the only way in this plan to lose existing data, so a backup precedes it.

### Write fence in the driver
A separate kext build (`PDAppleNVMeRW`), never in a default collection:
- the fence is `[first, last]` LBA of exactly one partition, found by the driver itself at start: primary and backup GPT must agree and pass their CRCs, the entry must carry the SiliconForest type GUID, the name, and the partition's unique GUID given in the boot arguments. Any mismatch: the build behaves as the read-only one.
- the gate gains Write and Flush (and optionally deallocate) on the I/O queue; Write/deallocate pass only if `first <= slba` and `slba + n - 1 <= last`, checked on the queue bytes as today; a second, independently coded comparison sits next to the doorbell. A counter `fence_rejected` must stay 0. The host test's random comparison is extended to the fence.
- two block devices: the whole disk stays write-protected (the GPT and every other partition cannot be written through any path); a second device exposes only the fenced range as its own writable disk, so a filesystem bug cannot address anything outside it.
- Linux' deferred-flush behaviour for this controller is ported with the write path.
- acceptance before first use on the M1's disk: the same build against a scratch target the owner chooses (an external disk is not possible with this controller, so: QEMU for the fence logic, then the real partition while it holds nothing).

### Boot chain
Today SiliconForest has only ever run as a guest of the m1n1 hypervisor, started by Python on a second Mac. Whether our kernel runs without the hypervisor (CPU start, SPRR/GXF, the virtual UART) is **not known** and is the largest open item; it belongs to the boot stream.
| Option | What changes on the disk | Needs from the owner | Risk | Undo | Cold boot (estimate) |
|---|---|---|---|---|---|
| B0 root on NVMe, still launched from the M5 | the SiliconForest partition only | P-step, go for the write build | none to the boot chain | stop using it | as today minus the RAM-disk upload |
| B1 (recommended next) stage-2 m1n1 on the EFI partition gains a native loader: default Linux unchanged, SiliconForest on a held key or a file flag | one file on the EFI partition (`boot.bin`), as was already done once for the boot window; kernel collection and device-tree overlay as files on the EFI partition | go for one file replacement; a loader in m1n1 (C; today the XNU loader exists only in Python) | a bad `boot.bin` stops Linux from booting until the saved original is put back from macOS or recovery; no boot-policy change | restore the saved `boot.bin` | iBoot ~7 s + m1n1 ~3 s + load ~1 s + kernel ~6 s + userland: **about 17–20 s** to a session, if the 10 s USB window is dropped for this path |
| B2 chain from U-Boot/GRUB (an EFI application that loads XNU) | files on the EFI partition, one boot menu entry | go for the files | lowest for Linux (its path is untouched); most engineering: XNU must be entered after U-Boot has taken over the machine, and the Apple device tree must be recovered | delete the files and the entry | B1 + U-Boot ~4–6 s: **about 23–26 s** |
| B3 own boot object: a second stub volume and boot policy, like a second Linux install | new APFS volume, **boot policy / Startup Security change** in recovery (`kmutil configure-boot` from 1TR) | physical presence in recovery, the machine password | touches the Secure Enclave's boot policies; a mistake does not harm the other systems' data but can require recovery to repair | delete the volume and its policy | as B1 (no key hold) |
None is irreversible in the strict sense. The steps that deserve the word in practice: any repartitioning without a backup, and B3's boot-policy change (cannot be tested beforehand, must be done at the machine).

### Open prerequisites (recorded, not started)
- **Does our kernel run without the hypervisor?** Never tried. What a safe test with the existing rig would need, with no disk involved: a launcher on the M5 that, like today's, RAM-loads the collection and the RAM root over USB into the m1n1 proxy but then enters XNU directly (m1n1's `chainload.py` already loads a Mach-O and jumps without the hypervisor) with iBoot's device tree and boot arguments. What the hypervisor does for us today and would have to be replaced or shown unnecessary: the console (virtual UART over the proxy's USB port: natively the guest owns that port, so the console must come from the real UART pins or the screen), secondary CPU start, SPRR/GXF (emulated today unless the registers are unlocked), the PMGR hooks that keep the proxy's USB alive, and the capture of faults. The bench's return path (external reset, Linux check) works unchanged. Risk: a hang with no console; cost: one external reset. Owner: the boot stream.
- **The 60 s root wait.** With a disk root that does not appear, this kernel calls `IOSetRecoveryBoot(BSD_BOOTFAIL_MEDIA_MISSING)`: it would set NVRAM `40A0DDD2-…:boot-picker-bringup-reason=missing-boot-media` and `boot-command=recover-system`, sync, and panic. From the sources: the only NVRAM controllers in the tree belong to the x86 platform experts; PDArmPlatformExpert registers none, nothing in the lab drives the SPI flash that holds the M1's NVRAM, and the hypervisor offers none. So under the hypervisor and natively alike the variables could at most land in the kernel's in-memory copy, and the code would then block forever waiting for the `IONVRAM` resource instead of panicking. That is inferred, not observed. No storage boot can reach it: all of them boot with `rd=md0`. Before any boot waits for a disk root: `wdt=-1` in its boot arguments and the call patched out of the kernel, so that it does not depend on the absence of a driver.

### Order proposed
1. Read-only driver proven (this ticket's phase 2).
2. Partition by the owner (P1), ext4 made from Linux, root file tree copied from Linux (Linux can mount it).
3. Write-fenced build proven on that empty partition; B0: root on NVMe, launched as today. This already gives "a real disk" and removes the upload.
4. Root-wait/NVRAM path settled, native loader (B1) once the kernel is shown to run without the hypervisor.

## Decisions for the owner
1. Go for the first controller start (nv02) after reading the observe boot.
2. P1 or P2, and the size.
3. ext4 (recommended) or APFS for the root.
4. B1, B2 or B3 as the target chain; B0 first in any case.

## Rules
Inherits SF-B. In addition: no boot with `pdnvme=2` or `3` without the host tests passing; after every such boot the Linux check (`linux_m1.sh all`: root mounted without recovery, no NVMe error, SMART compared); any anomaly creates `runs/m1-storage/STOP` and stops all storage boots.
