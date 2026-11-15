# Staged VideoToolbox compression and CPU pixel sessions

This combines whole Finch VTCompression.c, VTPixelSessions.c, the common session
code and VTH264.cpp with the retained PDAppleAVD decoder. OpenH2642.6.0 is built
from source on Linux/Nix (BSD notices shipped). Finch MIT/Apache notices remain.
No Mac build, Apple SDK header or downloaded firmware is used.

The port uses PD's real CFRuntime ABI. Shared VTSession property operations
route by CF type ID: existing decoder objects keep the existing implementation;
encoder/pixel objects use Finch's implementation. The unused Finch decoder kind
cannot be instantiated. Existing constant values and decoder source behavior
are retained; only absent constant symbols are added. Compression inputs are
bounded to8192x8192 before upstream integer products/allocations. C++ compilation
uses PD's libc++ headers and its CF enum/_Bool declaration compatibility flags.

Build `build.nix` and `tests.nix`. The public-API probe generates30 NV12 frames
at1280x720, requests no frame reordering and software selection, sets8Mbit/s,
30fps, GOP30, writes our pattern and Annex-B H264, checks ordered callbacks,
SPS/PPS and bounded AVCC sample lengths. It reports whether the hardware query
is supported; missing native property support is never called hardware=false.

Actual M1 prefix evidence (vt-encoder-stage-v2):30 ordered callbacks, hardware
query=false,42227 encoded bytes,3.43fps,7.940519 own-process CPU seconds,
8.744980 wall seconds. Unmodified ffmpeg bottle, no DYLD overrides, decoded all
30 frames and measured58.873634dB average PSNR against the generated source.
This measures an ordinary desktop session and the whole API/pixel-conversion
path, not an isolated OpenH264 core benchmark. Generic RGB-mediated pixel
conversion is slow; no realtime guarantee.

Same-byte Linux-built M5 public-API reference:30 ordered callbacks,36637bytes,
software requested. Native returns-12900 for the hardware-use query, so actual
native hardware/software selection is unknown. Native fps/CPU are recorded but
are not an M1 hardware-vs-software comparison. Only program stdout/stderr was
returned; its generated files were deleted with its fresh /tmp directory.
The first native probe queried hardware use before encoding and failed;
that receipt remains alongside the corrected one.

No AVE hardware encoding has occurred. RequireHardware currently returns the
actual unavailable error; no hardware capability is fabricated. The /dev/pdave hardware backend is paused by owner2026-11-15; the installed
software path remains independent.
This provider is now INSTALLED after the staged native/target key comparison,
1200 HW/SW decode PTS/hash matches, then the cold one-framework transaction.
All83 old exports remain,172total; four new deinterlace/field constants match
native M5. Requested field/deinterlace processing is explicitly unsupported,
not ignored. Three copied files were hashed before activation, .pre-finch is
retained, and600 hardware pictures, WAV/AAC HAL12000frames each, software
compression, desktop/dock/coreaudiod, Terminal/Activity Monitor open-a and
compat18/18 all pass. Main cold SHA256:
`dba1d23e6510187f9b8f6b1d6b9a2bf63362e6b91b9158bd8f0c79b2360f2ea9`.
No visual/acoustic or AVE hardware-encode claim.

Evidence: runs/sffinch/vt-encoder-stage-v2/{candidate,quality}.json,
m5-vt-encoder-v2-reference.json; provider pinned by the compiled probe's actual
derivation input (provider.json). Source and generated video are ours.

Installed evidence: runs/sffinch/install-VideoToolbox-v2/{qualified,regressions}.json;
vt-encoder-stage-v3/qualification.json, m5-vt-keys-v1-reference.json.
