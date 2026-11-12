## Goal
SiliconForest reads, and later boots from, the M1's internal NVMe storage instead of a RAM disk uploaded over USB. The internal disk holds the owner's macOS and Linux installs: **nothing in this ticket may modify it** until the owner authorises a specific write, and the code is built so that it cannot.

## Status (2026-11-11)
| Phase | State |
|---|---|
| 1 Facts | Sources, saved Apple device tree, hypervisor: done. Controller state as left by the boot chain: observed on the M1 (boot nv01, nothing written). Linux' view (read-only): partition layout, Identify, SMART baseline, kernel log, and reference SHA-256 of 30 ranges (1 MiB each: start incl. GPT, seven points across the disk, start/middle/end of every partition, end incl. backup GPT) in `runs/m1-storage/linux-a/`. |
| 2 Read-only driver `PDAppleNVMe` | Written, builds, in a collection; host tests of the read-only gate pass; booted once in observe mode (nv01). **The controller has not been started.** The first boot that starts it (nv02) waits for the reviewer's go. |
| 3 Filesystem groundwork | Source survey done; ext4 read-write on a 4096-byte-sector GPT disk and the behaviour of write-protected media shown in QEMU. |
| 4 Install design | Written below. Nothing of it is implemented or executed. |

Proven on the M1 so far: only what nv01 observed. Everything about reading is compile evidence and host tests.

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

SMART baseline, read from Linux (read-only passthrough) at two points with four ordinary bench cycles of other streams in between (each: Linux reboot, m1n1, a Darwin boot that never touched the controller, external reset, Linux boot): `power_cycles` 1588 → 1592 (**+1 per cycle**), `unsafe_shutdowns` 107 → 107 (the external reset of a stopped controller does not count), `host_write_commands` about +950 per cycle and `data_units_written` +231 in total (Linux' own shutdown and boot), `media_errors` 0, `percent_used` 4. Linux' kernel log of an ordinary boot shows no recovery, replay or orphan line; `p5` (ext4 `/boot`) and `p6` (btrfs) are mounted read-write on every boot, `p4` (EFI) too.

Expected from a boot that starts the controller under Darwin: `power_cycles` +1 as for any cycle (or +2 if the firmware counts its own start; to be measured), read counters up, **`unsafe_shutdowns` unchanged**, write counters changed only by what Linux does around it. The SMART log is also read inside Darwin at the start and the end of the session, which isolates our share exactly.

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
- independently the driver stops the controller itself `pdnvme_ttl` seconds after it became ready (default 90; the wrapper sets runtime − 150);
- `systemWillShutdown` does the same.
Not covered: a kernel panic or hang before the stop. The controller is then reset while enabled, as when the power button is held under Linux; with no write in flight there is nothing to lose, but `unsafe_shutdowns` would grow by one. It is checked after every boot.

### Boot plan (each step only after the previous one is clean; after each: M1 back in Linux, Linux root mounted without recovery, SMART compared)
| Boot | Level | Purpose |
|---|---|---|
| nv01 | 1 | observe: PMGR states, `CPU_CONTROL`, `CC`/`CSTS`, SART entries as left by m1n1. Writes nothing; does not start the controller |
| nv02 | 2 | **first controller start (needs the go).** RTKit, Identify vs Linux, hashes of the ranges through `/dev/pdnvme0`, throughput, latency, write refusal, SMART start/end, clean stop |
| nv03 | 3 | block device, GPT through the stack, same hashes through `/dev/rdiskN`, write attempts through the block layer, nothing mounted |
Before nv02 and after each: `tools/linux_m1.sh facts` and `hash` (ranges: start, GPT, seven points across the disk, start/middle/end of every partition, end). Ranges Linux itself changes between its two boots (inside its mounted filesystems) are reported separately: for those, equality with Darwin cannot be attributed.

### Not done in this driver
Interrupts (AIC 590; the engine polls, spinning only while a read is out), zero-copy DMA (everything is copied once), more than 8 reads in flight, namespaces 2 and 3, error recovery beyond "stop using the controller and shut it down", sleep.

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
Inherits SF-B. In addition: no boot with `pdnvme=2` or `3` without the host tests passing; after every such boot the Linux check (`linux_m1.sh facts`: root mounted without recovery, no NVMe error, SMART compared); any anomaly creates `runs/m1-storage/STOP` and stops all storage boots.
