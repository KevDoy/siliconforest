# Installing SiliconForest as its own entry in Apple's startup picker (route B3)

> **NOT AUTHORISED. NOT EXECUTED. NOTHING HERE HAS BEEN RUN.** This is a procedure on paper for
> the owner's decision (his instruction of 12 November 2026: SiliconForest gets its own entry in
> the startup picker, its own stub volume and its own first-stage m1n1 with our logo; the
> recovery-mode steps are done by him; the Linux/Omarchy install stays untouched). No script in
> this repository performs any step of it, and the scripts it names in section 4 are not written.
> It replaces the earlier procedure for route B1 (a selector in the shared `m1n1/boot.bin` on
> Linux's EFI partition), which is dropped: nothing is written to Linux's EFI partition, to
> Linux's stub, or to any file of the Linux install.

The model is the Asahi installer, which made the Linux entry on this machine. Its source was
read for this (`runs/m1-storage/ref/asahi-installer/src/stub.py`, `osinstall.py`, `main.py`,
`step2/step2.sh`, `build.sh`); file and function names below refer to it. Stage and kernel
packing: `DESIGN.md`. State of the disk root: `docs/roadmap/issues/SF-R-storage.md`.

## 1. What exists afterwards

| Thing | Where | Made by | Size |
|---|---|---|---|
| APFS container "SiliconForest" (the stub): volumes System (`SiliconForest`), Data, Preboot, Recovery | a new GPT partition | the owner, in macOS, with `diskutil` | 2.5 GB (the installer's `STUB_SIZE`; the paired recoveryOS image alone is about 1 GB) |
| the stub's boot object: **our stage-1 m1n1**, built with our logo, carrying one configuration line (`chainload=<PARTUUID of the boot partition>;sf/boot.bin`) | inside the stub (Preboot, placed by `kmutil configure-boot`) | the owner, in the stub's recoveryOS | about 1 MB |
| the picker's icon | `.VolumeIcon.icns` on the stub's System volume | the owner (the stub script) | under 1 MB |
| the SiliconForest boot partition: FAT32, one file `sf/boot.bin` = our stage-2 m1n1 (`m1n1-sf-stage.bin`) + configuration + the kernel collection (`pdsf_pack.py`), about 34.5 MB today, limit 60 MiB (`DESIGN.md` section 1) | a new GPT partition | the owner creates it; the file is copied by the owner from Linux or macOS | 512 MiB |
| the root: the SiliconForest partition as it is (ext4, label `SiliconForest`) | `p3` | unchanged in content | 64 GiB, or 61 GiB with option T below |

The Linux install's four pieces (its stub container, its EFI partition, `/boot`, the btrfs root)
and macOS's containers are not opened for writing by anything in this procedure, except for the
one resize of macOS's container in option M, which macOS does itself.

### Why two stages and a boot partition
The boot object in the stub can only be replaced from the stub's own recoveryOS
(`kmutil configure-boot`, physical presence, password). If it were the whole image (stage +
kernel), every new kernel would need a trip to recovery. As with Linux, stage 1 therefore
stays fixed and loads the changing part from a FAT partition. m1n1 reads FAT only, so that
part cannot live on the ext4 root; and it must not live on Linux's EFI partition. Hence a
small FAT partition of our own. (Variant without it, section 9, decision D2.)

## 2. Where the space comes from

Needed: 2.5 GiB (stub) + 0.5 GiB (boot partition) = 3 GiB = 786,432 blocks of 4 KiB.

**Option T: the tail of the SiliconForest partition** (the owner's stated source).
`p3` is blocks 192,633,862 to 209,411,077 (16,777,216 blocks). It becomes 192,633,862 to
208,624,645 (15,990,784 blocks, 61 GiB); the stub takes 208,624,646 to 209,280,005 (655,360
blocks) and the boot partition 209,280,006 to 209,411,077 (131,072 blocks). Nothing before
`p3` and nothing after it moves. What that costs:
- the ext4 filesystem is shrunk first (`e2fsck -f`, `resize2fs` to 15,990,784 blocks, from
  Linux, unmounted), then the GPT entry of `p3` is shortened and two entries are added. **That
  is a rewrite of both GPT copies**, the one operation of this procedure that touches a
  structure every system on the disk depends on. Linux's `sgdisk`/`parted` can do it while
  Linux runs from the same disk (the changed entries are not mounted); macOS's `diskutil`
  cannot shrink a partition whose filesystem it does not know, so the shortening is done from
  Linux and the APFS container is then created by `diskutil` in the free space
  (`diskutil addPartition`, as the installer's `action_install_into_free` does).
- **the write fence changes.** The fenced driver's range is compiled in
  (`m1-storage/kernel-rw/pd_nvme_wring.c`: 192,633,862 / 16,777,216), the host tests are
  written against it (`host-test/wgate_host.c`, `WRINGHOST`), and the boot arguments, the
  range list (`runs/m1-storage/linux-b/ranges`) and `private/disk-root/sf-part.env` carry it.
  All of them get the new length, the tests are re-run, and the first boot afterwards is a
  read-only one (`dr_desk_slot.sh` without `DR_RW`) before any write. Until then the old
  build must not boot: its fence would reach 786,432 blocks into the stub and the boot
  partition. (It would refuse to open: the driver verifies the GPT entry against its three
  arguments first. That check is the reason the order below is safe, not a reason to skip it.)

**Option M: macOS's container, as the installer did for Linux.** In macOS:
`diskutil apfs resizeContainer disk0s2 <3 GiB less>` (live, Apple's own operation), then two
`diskutil addPartition` calls in the freed space. `p3`, its filesystem, the fence and every
test stay exactly as they are. The cost is 3 GiB of macOS's 734 GiB container, and that the two
new partitions lie between macOS's container and `p3` (the GPT entries are added, none is
changed). **Recommended unless the owner wants macOS's size left alone**: it has no step that
this lab has not seen Apple's or the installer's tools do on this very disk, and it does not
touch the one partition that holds our data.

Either way the new entries get new index numbers; Linux's `nvme0n1pN` names of existing
partitions do not change as long as entries are added after the last used one (check with
`sgdisk -p` before and after; the lab's tools identify partitions by PARTUUID, never by number).

## 3. What the stub needs from Apple

Taken by the stub script from the macOS IPSW that matches the installed system firmware (the
installer reads single files out of the remote zip; 13.5 or later for this chip,
`CHIP_MIN_VER`), all into the stub, none into macOS:

| Component | Goes to | Why |
|---|---|---|
| `BuildManifest.plist` reduced to this board's identity (J293AP, chip 0x8103, `RestoreBehavior` Erase, variant "macOS Customer"), `RestoreVersion.plist`, `SystemVersion.plist`, `usr/standalone/bootcaches.plist`, `PlatformSupport.plist` | Preboot `/<volume group id>/restore/…`, System `/System/Library/CoreServices` | make the volume group a bootable "macOS" in the eyes of iBoot, the picker and `bputil`/`kmutil` |
| `BootabilityBundle/Restore/Bootability` and its trust cache, `Firmware/Manifests/restore/macOS Customer/` | Preboot restore bundle | personalisation of the boot chain for this volume group |
| every manifest item except `OS`, `BaseSystem`, `SystemVolume*`, `Cryptex*`, `RestoreRamDisk`, `RestoreTrustCache` (iBoot stage 2, device tree, SEP firmware, kernelcache, coprocessor firmware …) | Preboot restore bundle | what iBoot loads before it reaches the boot object; a few hundred MB |
| `arm64eBaseSystem.dmg` | Recovery `/<volume group id>/usr/standalone/firmware/` | **the paired recoveryOS** of this entry: `kmutil configure-boot` for a volume group is only accepted from its own recoveryOS |
| `AdminUserRecoveryInfo.plist`, copied from macOS's own Preboot | Preboot `/<volume group id>/var/db/` | which macOS users may authorise the policy change in recovery |
| `.IAPhysicalMedia`, `Finish Installation.app` with `step2.sh` | System volume | make the first start of the entry open recovery with the second step on screen |
| `.VolumeIcon.icns` + the `com.apple.FinderInfo` attribute, the bootable flag (`fsctl_set_bootable`) | System volume | the icon and the name in the picker |

Not taken: the installer's firmware collection for Linux (`all_firmware.tar.gz`, `vendorfw`),
its `asahi` directory on an EFI partition, its OS packages. The Wi-Fi, Touch Bar and other
firmware SiliconForest loads is already on its root.

## 4. What has to be written before this can be executed (none of it exists)

1. `sf-stub-install` for macOS: the installer's `StubInstaller` (`prepare_volume`,
   `install_files`, `collect_installer_data` without the firmware collection) with our name,
   icon, boot object and `step2.sh`; no distribution, no EFI partition, no questions about
   resizing. To be reviewed line by line against `stub.py` and run by the owner.
2. Stage 1: the pinned m1n1 with `BUILTIN_LOGO=siliconforest` (the Makefile takes
   `data/<name>_128.bin` and `_256.bin`, made from PNGs by `data/makelogo.sh`) and the
   release configuration (no USB proxy wait), and the configuration line appended the way the
   installer's `m1n1.build()` appends `chainload=`. It reads one file from one partition named
   by PARTUUID and writes nothing.
3. Stage 2 for installation: `m1n1-sf-stage.bin` in the variant that restarts the machine when
   it refuses or fails (today's falls back to the USB proxy), packed with the collection
   (`m1-storage/build-kc-native-rw-halt.nix`) and the boot arguments of section 6.
4. Proof from RAM first, as for every stage so far: the bench chain-loads stage 1 + the boot
   partition's content from RAM (`b1/tools/sf_stage_boot.sh` does this for stage 2 today:
   boots sfb06, sfb07, and the disk-root boots of step 4 in `SF-R-storage.md`).

## 5. The steps, and who does them

A. **Linux (owner, or the bench under his eye):** nothing for option M. Option T: shrink the
   filesystem and the entry of `p3` as in section 2; `e2fsck -fn`; one read-only SiliconForest
   boot with the rebuilt driver.
B. **macOS (owner, admin):** option M's `resizeContainer`; `diskutil addPartition` twice (APFS
   "SiliconForest" 2.5 GiB; FAT32 "SFBOOT" 512 MiB); run `sf-stub-install` (section 3's files,
   the icon, stage 1 as `boot.bin` inside `Finish Installation.app`, `step2.sh`); copy
   `sf/boot.bin` to SFBOOT. Then `bless --setBoot --mount <the stub's System volume>`: **from
   here until step D the default boot entry is SiliconForest's stub.**
C. **Recovery (owner, at the machine):** shut down; hold the power button until "Loading
   startup options"; choose SiliconForest; authenticate. The stub's recoveryOS starts and shows
   the second step (`step2.sh`): it checks that this is the paired recoveryOS in 1TR mode
   (`bputil -d`), then runs, each asking for the macOS password:
   - `bputil -nc -v <volume group id>`: Permissive Security **for this entry only**; macOS's and
     Linux's policies are separate objects and are not changed;
   - `kmutil configure-boot -c boot.bin --raw --entry-point 2048 --lowest-virtual-address 0 -v <System volume>`:
     our stage 1 becomes this entry's boot object.
   It then renames `.IAPhysicalMedia` away and restarts: stage 1 shows our logo, loads
   `sf/boot.bin`, stage 2 boots the kernel from it with the root on `p3`.
D. **Linux back as the default (owner):** at the next start hold the power button, select the
   Linux entry while holding Option ("Always Use"); or in macOS `bless --setBoot` on Linux's
   stub. Check: a plain restart lands in Linux.

## 6. Boot arguments in `sf/boot.bin`

`rd=uuid boot-uuid=<filesystem UUID of p3> pdnvme=3 pdnvme_part=<PARTUUID> pdnvme_part_lba=…
pdnvme_part_blocks=… pdnvme_write=1 pdnvme_readfence=1 pdnvme_ttl=0 pdcpuheld=1 cpus=8
pdthermal_delay=4 pd_quiet_wait=0 pd_rtc_wait=0` and the display/GPU words of the desktop
boots (`m1-storage/tools/dr_desk_boot.sh`). The stage refuses any command line whose root is
not exactly that filesystem UUID (`b1/src/pdsf_boot.c`, `PDSF_ROOT_UUID`). Decisions inside:
`pdnvme_ttl=0` (no time limit on the storage controller: an installed system must not stop
its disk after some minutes; the bench boots all had one), `pdpmu=2` or not (D4),
`pdpoweroff=1` or not (D5).

## 7. How Linux stays the default, and how SiliconForest is started

- Default: Linux, by step D. Nothing in SiliconForest changes the default: it has no NVRAM
  driver, and its restart (`reboot`, the menu's Session > Restart) is a plain SMC restart,
  which boots the default entry: **every restart of SiliconForest lands in Linux.**
- SiliconForest: hold the power button at start, choose its icon. (From Linux, the owner's
  own `asahi-bless --next <entry>` would do it without the button; that writes an NVRAM
  variable from Linux and is his tool and his choice, not part of this procedure.)
- A SiliconForest that hangs: hold the power button until off, press again: Linux.

## 8. Removal

In macOS: `diskutil apfs deleteContainer <the stub's container>`, `diskutil eraseVolume free
none <SFBOOT>`; the space is free again (option M: `diskutil apfs resizeContainer disk0s2 0`
grows macOS back; option T: grow `p3`'s entry and filesystem back from Linux, restore the old
fence constants). The entry disappears from the picker with the container. Its local boot
policy is stored in the system's own `iSCPreboot` under the volume group's id and becomes
unreferenced; what cleans it up, and when, is not known to this lab (the Asahi project
documents removal as deleting the partitions and nothing more). Linux's default status is
not affected by removal as long as step D was done.

## 9. Failure modes

| Seen | What it is | Way back |
|---|---|---|
| step B's `diskutil` refuses (no free space found, container busy) | nothing changed | fix and repeat; nothing to undo |
| the stub script stops half-way | an incomplete stub container; the default entry is still Linux (bless is its last step) | delete the container (section 8) and repeat |
| after `bless`, the picker does not show SiliconForest, or selecting it gives "macOS needs to be reinstalled" / a different recovery | stub incomplete or not paired (`step2.sh` says "did not boot into the correct recoveryOS") | hold power, choose Linux; in macOS repeat the stub script; default back to Linux with step D |
| `kmutil configure-boot` or `bputil` fails | wrong password, or not in 1TR (power button released too early) | `step2.sh` repeats the question; nothing is half-applied |
| our logo, then nothing | stage 1 could not read `sf/boot.bin` (missing, damaged, over 60 MiB) | hold power off, power on: the default (Linux after step D; **before step D this entry is the default: use the picker**) |
| stage 2 prints `PDSF: …` and restarts | it refused the image or the command line (wrong root UUID, checksum) | Linux; fix the file on SFBOOT from Linux |
| kernel starts, no desktop | as on the bench | hold power off, on: Linux. The root is then not clean: the journal is replayed at the next mount (proven: dr12 to dr16) |
| Linux does not start after step A (option T) | the GPT rewrite went wrong | Apple's recovery still starts (it does not depend on the GPT entries we touch); restore both GPT copies from the saved images (`runs/m1-storage/linux-b/`, taken before the write) with `dd` from recovery's Terminal. **This is the one failure of the list that can cost data, and only option T has it** |

Not covered by any of this: a mistake inside macOS's own `resizeContainer` (option M) is
Apple's code on Apple's container; a current backup of macOS and of the Linux home is the
answer to both options, and is the first thing to have.

## 10. Artwork (to be supplied; none of it exists)

| File | Used for | Form |
|---|---|---|
| `logo.icns` | `.VolumeIcon.icns`: the icon in the startup picker | Apple icon file with the usual sizes up to 1024 px, transparent background |
| `siliconforest_128.png`, `siliconforest_256.png` | m1n1's built-in boot logo (stage 1 and stage 2), converted by `data/makelogo.sh` to raw BGRA `_128.bin`/`_256.bin` | square, on black: the logo is drawn on the black boot screen, centred |
| the desk picture | already there (`chonkstep-darwin/backgrounds/siliconforest.png`): the kernel's boot picture until the first frame | unchanged |

## 11. Decisions asked of the owner

- **D1** space: option M (macOS's container, recommended) or option T (the tail of `p3`, with
  the GPT rewrite and the new fence).
- **D2** two stages with the SFBOOT partition (recommended), or one boot object holding stage
  and kernel (no extra partition; every kernel update is a trip to recovery).
- **D3** when: this needs him at the machine for steps C and D, about half an hour, and a
  backup first.
- **D4** `pdpmu=2`: let SiliconForest write the three PMU boot-state bytes Linux writes
  (`m1-native-boot/reboot/pd_pmu_scratch.h`). Not needed while every restart lands in Linux
  (Linux clears the counters); needed before a day on which only SiliconForest is booted,
  several times in a row. Never booted so far.
- **D5** `pdpoweroff=1`: allow Shut Down to switch the machine off (SMC `MBSE` = `offw`,
  `off1`). Never booted: on the bench a machine that is off needs a person. With D4 unset
  Linux's driver says the machine may restart instead of staying off (the PMU's shutdown flag).
- **D6** the artwork of section 10.

## 12. What this procedure deliberately does not contain

- No command in a form that could be pasted and run as a whole, and no script.
- No write by SiliconForest or by this lab's tools outside `p3`: the GPT, the new partitions,
  the stub and the boot policy are made by the owner's hands with Apple's tools and one
  script he has read.
- No change to Linux's stub, EFI partition, `/boot` or root, to macOS's volumes (beyond the
  one resize of option M), to NVRAM by us, or to the security policy of any other entry.
