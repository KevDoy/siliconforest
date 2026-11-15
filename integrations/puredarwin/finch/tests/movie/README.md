# Movie integration test

`movie.m` is our Objective-C test application, built on Linux by `build.nix`
using PureDarwin and our declarations. `fixtures.sh NEW_DIRECTORY` generates an
original 30-second H.264/AAC MOV and Linux NV12 frame hashes with FFmpeg.
`prepare.py --help` describes the isolated framework/reference packets. Rewrite
our test's public load names with `tools/portable-tests.py` before reference use;
run the identical bytes on both machines. Never build on the Mac or import an
Apple SDK.

The program reads compressed AVAssetReader samples into VideoToolbox and AAC
into ExtAudioFile. It retains bounded decoded buffers (512 MiB video, 64 MiB
audio), sorts decoded pictures by PTS, and then schedules them against the
retained PureDarwin AudioQueue sample clock. Its AudioToolbox provider forwards
queue operations to the existing HAL/coreaudiod path. This is a finite test,
not a streaming player implementation or an AVPlayer class port.

Modes:

- `--decode-only`: hashes and timestamps only; no output device or window.
  This is the permitted native-reference mode, with a 30-second timeout.
- `--headless`: sends decoded audio to AudioQueue and logs ordered video
  deliveries against that audio clock for 30 seconds.
- `--window`: creates our AppKit window and sets a hosted CALayer's contents
  to successive CGImages. `MOVIE_LAYER_SUBMIT` records submission, not a
  compositor presentation acknowledgement. TERM5 owns screen/input evidence.

`--software` disables the hardware preference. Every mode reports whether
VideoToolbox actually used hardware; preference alone is insufficient evidence.

On the installed M1 with `/dev/pdavd`, the second staged run passed all 900
hardware-decoded pictures and 1,440,000 stereo audio frames at 48 kHz. Every
ordered PTS/hash matched both Linux FFmpeg and the same Linux-built executable
on the M5. Audio completion took 29.943529 seconds, with an audio clock of
29.999979 seconds, a clock/wall difference of -56.450 ms, and maximum/mean video
delivery lateness of 60.000/9.887 ms. These are one-fixture headless measurements;
they establish neither acoustic output nor visible scanout. The window check
has been requested from TERM5.

The first M1 run failed before audio start: the AVAssetReader returned raw
composition timestamps including codec delay. Patch
`AVFoundation/patches/0006-full-track-video-edit-offset.patch` handles a full,
untrimmed video track's single unity-rate edit by mapping its first composition
time to asset time. It does not implement arbitrary trims, repeated edits or
rate changes. The failing receipt remains alongside the passing receipt.

Evidence (ignored): `runs/sffinch/movie-stage-v{1,2}/candidate.json`,
`movie-stage-v2/qualified.json`, `m5-movie-v1-reference.json`. Reproduce the
qualification with:

```sh
python3 integrations/puredarwin/finch/tests/movie/report.py \
  runs/sffinch/movie-stage-v2/candidate.json \
  runs/sffinch/m5-movie-v1-reference.json \
  runs/sffinch/movie-fixtures-v1/movie.nv12.framemd5 \
  --output runs/sffinch/movie-stage-v2/qualified.json
```
