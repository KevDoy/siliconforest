## Goal
Hardware video decode on the M1 (J293, T8103) through the SoC's Apple Video Decoder (AVD): a kernel driver ported from the Omacom Linux tree, a userland decode interface, a video player that uses it, and the decode side of VideoToolbox so that applications written for macOS (Firefox) get hardware decode.

Owner's words: "we want avd support, port that from linux and make sure we have hardware decoded video support properly, like for playing videos etc, in firefox, and lets get a video player working that uses avd".

## Status (2026-11-12)
| Layer | State | Proof |
|---|---|---|
| 1. Facts | done | below |
| 2. Kernel driver `PDAppleAVD` | **works on the M1**: H.264, HEVC and VP9 (8-bit 4:2:0), every decoded frame bit-identical to a software decode | boot `av02`: 1,290 of 1,290 frames in 10 clips |
| 3. Userland interface `/dev/pdavd` + `libpdavd` + FFmpeg hwaccel `pdavd` | works for decode (the proof of layer 2 runs through it); buffers reach programs as read-only mappings, **not** as dma-bufs | boot `av02`; see "Not done" |
| 4. Player | see "Player" | |
| 5. VideoToolbox | see "VideoToolbox" | |

"Works" above means: a program ran on the M1 under our kernel and its output was compared with a reference made elsewhere. Nobody has looked at a decoded picture on the panel in the boots listed under layer 2 (they have no desktop).

## Facts
- **Hardware** (Apple device tree `/arm-io/avd`, Linux `avd@269080000`): one `reg` 0x268000000 + 0x1404000; the driver uses `code` +0x1080000 (0xc000: program memory of the block's Cortex-M3), `mbox` +0x1098000, `ctrl` +0x1100000 (decode control and the instruction FIFOs). Power: `clock-gates` 0x12b 0x12c 0x12d. Interrupts AIC 540, 541 (541 is mailbox 1, the one used). Own DART `/arm-io/dart-avd` at 0x269010000 (T8020 type, 16 KiB pages, `clock-gates` 0x145, AIC 547), stream 0.
- **Firmware: yes, and it is open.** The Cortex-M3 runs the MIT-licensed firmware of github.com/aurora-silicon/avd-fw (Arch package `avd-fw 0.1-1` on the M1's Linux). T8103 image: `apple/avd-fw-v2-t0.bin`, 49,152 bytes, sha256 `0c80d2a0…5346ad`. The driver copies it into the `code` region at every start, as Linux does. It is **not in the repository**: copied read-only from the M1's Linux into `private/firmware/apple/` and staged per boot as `/Library/Firmware/apple/avd-fw-v2-t0.bin`, like the Wi-Fi and Touch Bar firmware. Because it is MIT source, it could instead be built and shipped from source (clang + lld, `make AVD_VER=2 AVD_TIER=0`); that build was not done here (the clone is in `runs/avd/fw-src`): the owner's decision.
- **Kind of decoder**: stateless. Linux exposes it as a V4L2 stateless ("request API") decoder: userspace parses the bitstream and passes parameter structures per picture; the driver turns them into an instruction stream for the block. On the Omacom system the userland is `libva-v4l2_request-avd` under FFmpeg's VA-API hwaccel (also used by mpv and Chromium there).
- **Codecs in the Linux driver for T8103**: H.264 (also 4:2:2 and 10 bit, no interlace, no slice groups), HEVC (Main, Main 10), VP9 (profiles 0–3 by the tables). **No AV1 on this SoC** (the block of T8122 and later has it). Maximum 4096x4096 for H.264 and VP9. Proven here: 8-bit 4:2:0 only, up to 1920x1080.
- **Hypervisor**: nothing special. Our boots run as a guest of the m1n1 hypervisor, which passes the block's registers, its DART, its power domains and AIC interrupt 541 through; no launcher change was needed.
- **Patents**: H.264/AVC, H.265/HEVC and VP9 are involved. Nothing was held back for that reason.

## Layer 2: `PDAppleAVD.kext` (`integrations/puredarwin/m1-avd/`)
- **A port, not a rewrite**: the Linux codec files (`avd-h264.c`, `avd-hevc.c`, `avd-vp9.c`), the hardware file (`avd-hw.c`), the instruction definitions and the V4L2 helper libraries they use (`v4l2-h264.c`, `v4l2-vp9.c`) are in `kernel/linux/` **byte-identical** to the Omacom tree at `6a08a08e` (the host test checks this against the tree). They compile against a small stand-in for the Linux interfaces they use (`kernel/shim/pd_linux_shim.h`). `kernel/pd_avd_core.c` is `avd-drv.c` + `avd-v4l2.c` without V4L2: buffers, job tables, FIFO submission, the mailbox interrupt protocol, the 2 s watchdog with a reset of the block. `kernel/PDAppleAVD.cpp` is the platform side (power, mappings, firmware file, interrupt, device node); `kernel/pd_avd_dart.cpp` maps every buffer in a 2 GiB address space behind the decoder's DART.
- **Off by default**: does nothing unless `pdavd=1` is in the boot arguments (`2`: info lines, `3`: debug). Even then nothing is powered before the first open of `/dev/pdavd`. GPL-2.0; notices in `NOTICE`, `Info.plist` and every file.
- **Collection**: `build-kc.nix` = the current default collection (`m1-boot-speed/build-kc.nix`) + this kext (185 kB before stripping). Booted as `/nix/store/rzryn4avl3hcy31hjx5lakmkhdgaqfw4-puredarwin-kc-arm64-0.1`. **Proposed for the default collection** (it is inert without `pdavd=`); not put there by this stream.
- **Host tests** (`host-test/run-host-tests.sh`, no hardware): (1) the Linux files are identical to the tree's; (2) core + codec files build with `-Wall -Werror` under ASan/UBSan against a stand-in decoder; (3) every recording replays with all decodes succeeding, no sanitizer report, no device memory left, and a repeatable instruction stream. All pass for the ten clips. What this is and is not: because the generators are the Linux files themselves, "bit-identical instruction streams" holds by construction for the generators; the test does **not** compare against streams captured from a running Linux (that would have needed playback on the M1's Linux, which was not allowed tonight). The proof that the streams are right is the hardware result below.
- **Hardware result, boot `av02`** (hypervisor boot, no desktop; `tools/avd_boot.sh`): each clip was parsed on the workstation by FFmpeg's `pdavd` hwaccel into a recording of the driver calls (`PDAVD_DEVICE=record:<file>`), the recording was replayed on the M1 into the real decoder (`pdavd-replay`), and the MD5 of every decoded picture (visible NV12 area) compared with `ffmpeg -pix_fmt nv12 -f framemd5` of a software decode on the workstation (`tools/avd_compare.py`).

| Clip | Stream | Frames matched | Decode time per frame, mean / max |
|---|---|---|---|
| bbb-1080p-h264-10s | H.264 High, CABAC, B-frames, 1920x1080 | 300 / 300 | 2.75 / 7.40 ms |
| bbb-1080p-h264-cbp-nob-3s | H.264 Constrained Baseline (CAVLC) | 90 / 90 | 2.57 / 4.68 ms |
| bbb-1080p-h264-main-nob-3s | H.264 Main, no B-frames | 90 / 90 | 2.89 / 7.09 ms |
| bbb-1080p-h264-main-b-3s | H.264 Main, CABAC, B-frames | 90 / 90 | 2.87 / 7.25 ms |
| bbb-1080p-h264-cqm | H.264 High, custom scaling matrices, 8x8 transform | 60 / 60 | 2.84 / 6.85 ms |
| bbb-720p-h264-slices4 | H.264 High, 4 slices per picture, weighted prediction, B-pyramid | 60 / 60 | 1.39 / 3.56 ms |
| bbb-480p-h264-cavlc-manyslices | H.264 Baseline, 854x480 (cropped), 5 references, 134 slices in 60 pictures | 60 / 60 | 0.65 / 2.10 ms |
| bbb-360p-h264-5s | H.264 High 640x360 | 150 / 150 | 0.45 / 1.29 ms |
| bbb-1080p-hevc-10s | HEVC Main 1920x1080 | 300 / 300 | 2.34 / 3.76 ms |
| bbb-1080p-vp9-10s | VP9 profile 0 1920x1080 | 300 / 300 | 2.52 / 7.73 ms |

  1,290 of 1,290 frames; 0 decode errors, 0 watchdog timeouts, 0 DART faults, 2 interrupts per picture. Decode time is from submission to the decoder's "done" interrupt, including the wake-up of the waiting thread. The M1 returned to verified Linux. (Boot `av01` before it proved only that the kext loads: its wrapper did not select the guest script.)
- Content: Big Buck Bunny (Blender Foundation, CC BY 3.0), cut and re-encoded on the workstation (`player/tools/make_clips.sh`; three more variants in `runs/avd/t`, commands in this stream's log). Nothing of it is committed.

## Layer 3: `/dev/pdavd`, `libpdavd`, FFmpeg hwaccel
- **`/dev/pdavd`** (`include/pdavd/pdavd_ioctl.h`, five ioctls): create a context for a codec and size with N decoded buffers; map a decoded buffer read-only into the caller; decode (parameter structures + coded data + destination buffer + a timestamp that names the picture for later references); destroy. The parameter structures are Linux's `struct v4l2_ctrl_*` (`include/pdavd/v4l2-controls.h`, the unchanged uapi header, which is dual-licensed BSD-3-Clause).
- **`libpdavd`** (`user/pdavd.c`, MIT): thin wrapper, plus the recording backend (`PDAVD_DEVICE=record:<file>`) used by the tests.
- **FFmpeg hwaccel `pdavd`** (`ffmpeg/0001-pdavd-hwaccel.patch` against FFmpeg 8.1.2, LGPL): hardware device type `pdavd`, pixel format `AV_PIX_FMT_PDAVD`, hwaccels `h264_pdavd`, `hevc_pdavd`, `vp9_pdavd`. FFmpeg does the parsing, reference lists, reordering (display order) and timestamps; the control filling is adapted from the V4L2 request API hwaccels (Jonas Karlman's FFmpeg branch), which is the code the Linux driver is used with. Any FFmpeg program gets hardware decode with `-hwaccel pdavd` / an hw device of that name; frames are downloaded with `av_hwframe_transfer_data()` (NV12). Colour metadata (matrix, range, primaries, transfer) is FFmpeg's, from the stream.
- Builds: `build-user.nix` (libpdavd, `pdavd-replay`), `build-ffmpeg-pdavd.nix` (FFmpeg for our userland with the hwaccel: `/nix/store/a2828v5xbwwyysh35mq0j2r061jzfbga-darwin-lab-ffmpeg-arm64-darwin-pdavd-8.1.2`).

## Not done / known limits
- **No dma-buf export, so no zero-copy to the compositor or Mesa.** Decoded buffers are kernel memory mapped read-only into the process; a player copies each frame (about 3 MB at 1080p) into a wl_shm buffer or a GL texture. Zero-copy needs the decoded buffers exported as dma-bufs that the asahi DRM driver can import (PRIME import in the LinuxKPI layer of `m1-drm72`) and NV12 import in the compositor: not started.
- **The device node is root-only (0600)**: the Linux codec files trust the parameter structures to the degree V4L2's control framework validates them, and that validation is only partly ported (sizes and array bounds are checked; field ranges are not). Opening it to unprivileged users needs that validation.
- 10-bit (P010) and 4:2:2 output are in the driver but nothing was decoded in them; the FFmpeg hwaccel is offered for 8-bit 4:2:0 only.
- One decode at a time (as Linux's mem2mem); several contexts can exist (8). No runtime power management: the block is on while a context exists.
- The watchdog/reset path (PMGR reset of `AVD_SYS`, DART reprogramming) has never run: no decode ever timed out.
- Interlaced H.264 is refused by the driver (as in Linux). Resolution changes inside a stream need a new context (FFmpeg does that).
- Conformance: the JVT/ITU conformance suites were not run through the hardware yet (the clips above are encoder output of x264, x265 and libvpx).
