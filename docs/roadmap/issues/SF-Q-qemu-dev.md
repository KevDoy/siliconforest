# SF-Q: SiliconForest in QEMU (development environment)

## Goal
Userland work (compositor and shell, terminal, settings menu, themes, audio server, session
scripts) is proven on this workstation in QEMU, in under a minute per run and without the one
M1 on the bench; the M1 is needed only for the final hardware proof. Second goal (owner,
11 November): a full SiliconForest OS installed in QEMU, on a disk that persists.

How to use it, what the guest contains and the coverage table:
`integrations/puredarwin/qemu-dev/README.md`. Tools: `tools/qemu_desktop.sh` (throw-away loop),
`tools/qemu_install.sh` (persistent install).

## Decision: arm64 `virt` under TCG is the primary target
| | arm64 `virt`, TCG | x86_64, KVM |
|---|---|---|
| Our binaries | run unchanged (same compositor, terminal, coreaudiod, libSystem as the M1) | all of it would have to be rebuilt for x86_64: Rust target, linker wrapper, PDGOP/IOKit/xkbcommon statics, sysaudio, the libSystem patch |
| Display | ramfb through the firmware's GOP: IOGOPFramebuffer, the PDGOP presenter the CPU renderer already uses | `-vga std` through GOP: the same |
| Input | xHCI USB keyboard/mouse/tablet: PDHID nodes, `--input pdhid` as on the M1 | the same, plus PS/2 |
| New compositor backend needed | none | none |
| Measured here, upstream image unmodified | DEBUG kernel, 4 vCPU, 4 GiB: shell prompt after 85 s (host load 7) | RELEASE kernel, 2 vCPU, 4 GiB, upstream `image-minimal`: kernel banner at 14.6 s (the firmware takes that long), root mounted 17.7 s, launchd 24.9 s, console login started 42 s; no zsh prompt on the serial line within 120 s in two runs |
| Measured here, tuned | session script running at 31-38 s, desktop with two terminals at 45-60 s (host load 12-24) | not tuned |
| Exercises arm64 code | yes | no |

x86_64 with KVM was expected to be the fast one and was not, out of the box: its userland start
is no quicker than the tuned arm64 guest's. It stays a possible later target for a CI-style
check; nothing of ours is built for it.

Where the arm64 time went and what was done: 40 s were XNU's `IOKitInitializeTime` waits
(patched in the kernel binary, as `PCIE_TIMEWAIT` does on the M1); `pmap_startup` grows with
memory (4 GiB: 10 s, 1 GiB: 2.6 s, with a DEBUG kernel); the DEBUG kernel (replaced by
upstream's RELEASE `virt` kernel); waiting for the zsh login before starting anything (the
session is a launchd job now). What is left is userland process start-up: about half a second
per `exec` under emulation, and QEMU's CPU time is about equal to wall time, so other builds on
the host stretch every number here.

## State (11 November)
| Item | State |
|---|---|
| Desktop in QEMU from one command, headless, autopilot, screenshots, pass/fail | works: `tools/qemu_desktop.sh` |
| Time from command to desktop | 45 s (load 12) to 60 s (load 24); image staging 0.7-1.7 s of it. Under 60 s only on a host that is not busy |
| Window mode | see "Not proven" |
| Resolution | 1024x768 (the firmware's largest ramfb mode) |
| Build from a source tree (`--src`) | see "Runs" |
| Persistent install: create, boot, update, reset | works with the minimal userland; state and files survive reboots (shown) |
| Full userland in the install (`--variant wayland`) | recipe written; upstream's full no-X image was still building when this was written; not booted |
| Network in the guest | not working: see open items |

## Runs
- `runs/qemu-dev/t3`, `t4`: autopilot `settle,pointer,drag,type` with the M1's current binaries
  (wm-darwin-scale-desktop-build-007, terminal-application-build-008): PASS,
  `WMAUTO drag ... result=moved`, `WMAUTO type ... result=client-redrew-after-typing`,
  `WMAUTO done result=ok`; screenshot `runs/qemu-dev/t3/shots/t004.png`.
- `runs/qemu-dev/install`: create; boot headless 40 s (desktop at 1024x768, rc 0); boot with a
  command writing `/Users/sf/note.txt` and a theme into the state directory; next boot reads both.
- `runs/qemu-dev/full1`: compositor built from `sources/chonkstep-darwin` as it was in the
  working tree, full autopilot. Result: see the stream's final report (not known when this was
  written).

## Found on the way
- Upstream bug: with a generic USB HID keyboard (QEMU's `usb-kbd`), `/dev/usb_hid_kbd` is never
  published (the LED-handler registration sets up the queue, after which the lazy publish is
  skipped). Fixed for this target in `pdhid-publish-keyboard.patch`; worth sending upstream.
- The PDHID nodes appear only with the first event of each kind: `qd_run.py` sends one Shift
  and one pointer step when the session starts.
- A busy host panics the guest with a ticket-spinlock timeout unless `no_lock_timeouts=1`.
- `-cpu max` hangs in the firmware; `neoverse-n2` works.
- This AAVMF build has no bochs/VGA driver, this QEMU no `virtio-gpu-pci`: ramfb is the display.
- `e2fsck` cannot reopen a filesystem at an offset inside an image after journal replay; the
  root partition is copied out and back (`qd_image.py --fsck`).

## Not proven
- Window mode on the owner's screen: not opened there (headless is the rule unless asked).
  Whether it was exercised on a private X server is in the stream's final report.
- The theme bug (theme switch from the menu sometimes not applied, window controls wrong): not
  reproduced here yet. The autopilot's `settings` phase picks themes and is the place to start:
  `tools/qemu_desktop.sh --only settle,settings`.
- Anything in the "no" rows of the README's coverage table.
- Full-userland install, network, HTTPS, a desktop running as the user `sf`.

## Open items
1. Network: the virtio interface is probed (`IOVirtIONet: MAC ...`) but never named, so there is
   no `en0` (waited four minutes). Nothing in the minimal userland names interfaces (on the M1
   the Wi-Fi driver asks IONetworkStack itself). Either configd's InterfaceNamer from the full
   userland, or a small tool that asks IONetworkStack to register all interfaces. After that
   `qemu-net-up` (DHCP by `pd-dhcp`, then `mdnsd`) is expected to work as on the M1; untested.
   HTTPS and CA certificates need curl/openssl, which only the full userland has.
2. Full userland: finish the `image-arm64-virt-wayland` build, `qemu_install.sh create --variant
   wayland`, boot it, remove what collides with our session (upstream's own
   `org.puredarwin.wayland-session` job is already removed by `create`).
3. Larger framebuffer: needs a firmware whose ramfb driver has more modes (an edk2 build), or
   the virtio-gpu path (a QEMU with `virtio-gpu-pci` and a presenter that calls PDGOPPresent).
   2560x1600 would give the M1's real geometry at 200%.
4. No reboot per edit: keep the guest up and replace the compositor through a second disk or
   the network, then restart the session job. Would take the loop from about 50 s to a few.
5. Sound: a device coreaudiod can open (upstream has `RavynHDAudio`; our server talks to the
   M1's driver interface), so the volume phases stop reporting SKIPPED.
6. Multi-user: launchd already runs the system multi-user (daemons, console job), but there is
   no login, no su, and passwords are unset; the console shell and the desktop are root's. A
   desktop as `sf` needs a way to drop privileges before the compositor starts and a check that
   PDGOP, the PDHID nodes and the PTYs work for a non-root user.

## What a "full OS" in QEMU still lacks (for a user, not as work started)
- A login: no `login`/`getty`, no passwords, no `su`/`sudo`; root shell on the console.
- A package tool (the Homebrew stream), compilers and build tools, git, curl, ssh (all in
  upstream's full userland, none in the minimal one).
- Man pages, `ifconfig`, `netstat`, `ps` beyond toybox's, a pager other than `more`, an editor
  other than `vi`.
- Network configuration as a service (configd, DHCP, resolver) and time (no RTC driver on
  `virt`: the clock starts wherever the firmware leaves it).
- A clean shutdown from the desktop (the session ends by time or by closing the window; the
  disk is kept consistent by `sync` and a host-side filesystem check).
- Applications: one terminal. No file manager, browser, text editor with a window, settings
  beyond the desktop menu.
- A display larger than 1024x768, GPU rendering, sound output, clipboard or file sharing with
  the host.
- Swap, more than 512 MiB of root (the minimal image's partition), a separate `/Users` volume.
