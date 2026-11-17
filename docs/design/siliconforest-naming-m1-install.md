# Installing the renamed system on the M1: plan (not executed)

State: 17 November 2026. Written without access to the M1. Nothing here was run. What is
inferred from the tree and not read from the machine is marked *(inferred)*.

The old names in this note are meant: it describes the installed system as it is now.

## Why this is one switch and not a file-by-file update

Kernel and user space find each other by name: IOKit class names (`IOServiceMatching("PDDRMDri")`
in Mesa's and the compositor's device glue), device nodes (`/dev/pdavd`), sysctls
(`kern.pd_avd.*`), boot arguments read back from `kern.bootargs` (`pdm1tpscroll=`), the Wayland
protocol `pd_window_position_v1` between AppKit and the compositor, launchd labels. A renamed
kernel with the installed user space, or the reverse, has no GPU desktop, no video decoder and
no window placement. So the unit of installation is **the kernel collection plus every installed
binary that carries one of those names**, switched together, and the unit of rollback is the
same pair.

## What changes

| Where | Now | Renamed | Source |
|---|---|---|---|
| SFBOOT `sf/kernel.bin` | configuration + collection "48" with `PD…` kexts, `org.puredarwin.*` bundle ids; its command line carries `pdm1kbd=1 pdm1trackpadenum=1 pdm1trackpadchild=1 pdgpudt=2 pdagx=1 pddispdt=2 pddcp=2 pdnvme=3 pdnvme_part=… pdnvme_write=1 pdnvme_readfence=1 pdcpuheld=1 pdapciebridge=1 pdavd=1 pdtextboot=0 …` | the same with `SF…`, `org.siliconforest.*`, `sfm1kbd=1 … sfavd=1` | `m1-native-boot/install/build.sh` (the command line is packed into `kernel.bin` with the collection) |
| SFBOOT `sf/boot.bin` (stage 2) | unchanged | unchanged: stage 2 passes the packed command line on and knows no kext or argument name *(inferred from `build.sh` and `src/pdsf_file.c`; to be confirmed by comparing the two builds' `boot.bin` byte for byte)* | |
| stage 1 in the stub volume | unchanged | unchanged (a new stage 1 needs recovery mode; none is needed) | |
| `/System/Library/CoreServices/SystemVersion.plist` | ProductName `PureDarwin` | `SiliconForest` | upstream `image.nix` |
| host name | `puredarwin` (`/etc/hosts`, the job `org.puredarwin.hostname`) | `siliconforest` | upstream `image.nix`. The M1's SSH host key does not change; `known_hosts` entries are by address |
| `/System/Library/LaunchDaemons`, `/Library/LaunchDaemons` | `org.puredarwin.hostname`, `.dmesg-save`, `.root-remount`, `.dbus`, `.desktop`, `.wayland-session`, `.cocoa-session`, `.sshd`, `.pd-netd` | `org.siliconforest.*` (and `.sf-netd`). The old plist files are **removed**, not left beside: two jobs would start the same daemon | upstream `image.nix`; `m1-bwfm/netd`; `desktop/system` |
| libSystem, dyld, libobjc, CoreFoundation, Foundation, AppKit, the other frameworks | symbols and strings with `pd_`/`PD` (`pd_cgl_*`, `PDSurface`, `F_PD_SAMEFILE`'s users, `DYLD_PD_MISSING_OK`) | renamed | the fork tree; our Cocoa patches |
| Mesa (GL, Vulkan), libdrm glue, `libPDSurface`, `libPDGOP` | class and library names | `libSFSurface`, `libSFGOP`, … | `m1-mesa-asahi`, `m1-vulkan`, upstream |
| Compositor, dock, terminal (`chonkstep-darwin-desktop` and `.next`, `chonk-terminal-darwin`) | display make `PureDarwin`, `PDGOP`/`PDHID` seats, `pd_window_position_v1`, reads `pdm1tp*=` | renamed | `sources/chonkstep-darwin` + `terminal-app/compositor/*.patch` |
| Daemons and tools | `pd-netd`, `pd-dhcp`, `pd-net-up`, `pd-backlight`, `pd-wifipm`, `pdplay`, `pdtone`, `pdvolume`, `pdvideo`, `pdtbtest`, `pdpowerd`, `pdaccountsd` | `sf-…`, `sf…`. Old binaries are removed after the switch is confirmed | the recipes of `tools/staged.sh` |
| Audio server and its frameworks | the **pin**'s build (upstream `e7aa00a6`), old names: `/dev/pdaudio`, `/dev/pdspeaker`, `kern.pd_sound.*`, `kern.pd_speaker.*`, Mach service `org.puredarwin.audio.coreaudiod`, `/Library/Preferences/org.puredarwin.audio.plist` | the **tip**'s build with our `/dev` backend and the new names only (`/dev/sfaudio`, `/dev/sfspeaker`, `kern.sf_sound.*`, `kern.sf_speaker.*`, `org.siliconforest.audio.coreaudiod`, `org.siliconforest.audio.plist`). This is also a change of server generation (upstream's reworked `coreaudiod`), which the M1 has not run; checked in QEMU as far as a machine without sound hardware can (SF-I, "The renamed audio server"). `coreaudiod`, CoreAudio, AudioUnit and the AudioToolbox family go in together | `m1-sound/sysaudio`, `m1-sound/build-sfplay.nix`; the session script `m1-storage/system/sf-session` (it sets `kern.sf_speaker.hold_probe=1`) |
| Firefox, VLC, the video player | media glue names `/dev/pdavd`, `AV_PIX_FMT_PDAVD` | renamed; a rebuild of Firefox | `firefox/`, `finch/`, `m1-avd` |
| Apple's Terminal and Activity Monitor | not ours, no PD name | unchanged; the libraries they load are ours and are replaced | |
| `/usr/share/doc/siliconforest/NOTICE` | absent | new | `NOTICE` |
| neofetch | patched for `PureDarwin` | patched for `SiliconForest` | `desktop/neofetch` |

State under `/private/var/db`: the names in the tree are already `sf…` or neutral
(`desktop-state/chonkstep/…`, `sffx`, `sfvk`, `sfmetal`, `sft12`, `sfavd`, `sf-bench`,
`sf-apps-stage`); none is keyed by a `pd` name *(inferred from the tree; to be confirmed on the
machine with the scan below)*. launchd keeps no database by label on this system *(inferred)*;
disabled-job overrides, if any exist under `/private/var/db/launchd.db`, are keyed by the old
labels and would have to be carried over by hand.

Exact file list: not known without the machine. It is produced, on the M1 or on a copy of its
root, by

    # every installed file that carries an old name outside attribution text
    find / -xdev -type f -size -200M 2>/dev/null | while read -r f; do
      strings -a "$f" 2>/dev/null | grep -v -i -E 'copyright|license|all rights reserved' |
        grep -q -E 'puredarwin|PureDarwin|org\.puredarwin|PD[A-Z][a-z]+[A-Z]|(^|[^a-z])pd_[a-z]' && echo "$f"; done

and the same scan after the switch must print only files that are not ours (Apple's two
applications, Firefox's bundled third-party libraries, recorded evidence under
`/private/var/db`).

## Order

1. **Build what is installed** (workstation): every staged piece and the Cocoa, Finch, Firefox
   and Homebrew-overlay builds from `sf-rename`. The audio server builds at the tip from the
   renamed tree (17 November, SF-I; file list in `runs/m1-coord/done-AUDIO`).
2. **Launcher** (only if a hypervisor boot is wanted as a rehearsal): the launcher base tree's
   member gate names the old kexts; change it there and regenerate the launcher patches
   (`docs/design/siliconforest-naming.md`, exceptions). A disk boot needs none of this.
3. **QEMU rehearsal** of the core files (dyld, libSystem, libobjc, CoreFoundation, Foundation)
   as for any core change (`core.lock` pipeline, SF-Y), from `sf-rename`.
4. **Stage on the M1, running the old system**: copy the renamed user space into
   `/private/var/db/sf-rename/new/` as a tree of final paths, with a manifest (path, sha256). Nothing
   live is touched. The M1 keeps running.
5. **Switch script** on the M1 (`sf-rename-switch`, to be written; run from the text console or
   over SSH with the desktop stopped): for each file of the manifest, move the live file to
   `/private/var/db/sf-rename/old/<path>`, move the new one in; move the old launchd plists to
   `old/`; `sync`. The reverse direction is the same script with `old` and `new` exchanged. Files
   every program loads (dyld, libSystem) go last and through the guarded installer of SF-Y.
6. **Kernel**: from the M1's Linux, `sf-bootbin-install.sh <PARTUUID> boot.bin kernel.bin` with the
   renamed kit. It keeps the present pair as `boot.bin.old`/`kernel.bin.old`.
7. Boot SiliconForest (`asahi-bless --next`), check the surfaces of the naming note, section 6.

Steps 5 and 6 are done in one sitting, 5 first: between them the machine is in Linux.

## The rollback entry

- `sf/kernel.bin.old` is collection 48 **with its own command line packed inside**: it reads its
  `pd…=` arguments from itself. Nothing has to be translated, and no boot argument is passed from
  outside (stage 1 passes two fixed lines, stage 2 refuses others). `sf-bootbin-install.sh
  <PARTUUID> --rollback` from Linux brings it back, as today.
- Kernel 48 needs the **old user space**. After a kernel rollback the system comes up with the
  renamed user space: the text console, launchd, sshd and Wi-Fi are expected to work
  *(inferred: none of them finds the kernel by a renamed name; `sf-netd` uses BSD ioctls by
  number)*, the desktop is not. Then run the switch script in the reverse direction on the M1
  and reboot.
- If the machine does not come up far enough for that: from Linux, mount the SiliconForest
  partition read-write for this one purpose (allowed with the reason recorded in the log;
  `e2fsck -fn` first), run the reverse switch against the mounted tree, unmount, `e2fsck -fn`.
- Keep `/private/var/db/sf-rename/old/` and `kernel.bin.old` until the renamed system has run for
  as long as the owner wants; then the old set is deleted in one step, and the next kernel
  install makes the renamed kernel the `.old` one.

## Not changed by this plan

Partition table, NVRAM (default boot stays Omarchy), stage 1, the write fence (its constants
are the same numbers under the names `SF_FENCE_FIRST`/`SF_FENCE_BLOCKS`), the halt gate, the
SSH host key, the owner's desktop state files.
