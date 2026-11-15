# Finch AVKit, prefix stage

Build `build.nix` and `tests.nix` with the Linux/Nix toolchain. This ports the
whole Finch `userland/AVKit/AVKit.m` against our own declarations and the staged
Finch player sources in `../AVFoundation/player`. No Apple SDK input.

The Linux-built value probe matched native M5 and M1 output: player assignment,
control-style defaults, route-picker properties and PiP support query. The
M1 reports no OpenGL context and uses ordinary view fallback; this test proves
no video rendering, audio, fullscreen presentation or picture-in-picture.
The first M1 probe overlapped a newly announced TERM6 idle measurement; this
was disclosed. Its functional result is retained, not uncontended timing.
A fresh07:26Z rerun under gui.lock repeats the functional pass; neither run
establishes rendering or a timing benchmark.

Finch's player here provides clock/state and JPEG/ImageIO handling. It has no
H264 VideoToolbox or AudioToolbox output backend. It is therefore staged only;
it does not replace the installed AVFoundation or our separate working movie
pipeline. PiP explicitly reports unsupported.

QuickTime's reviewed inventory now has 6/18 AVKit imports provided: AVPlayerView
class/metaclass, AVAssetIdentifier, AVTimecode and the two slow-motion range
conversion functions. The actual export audit is
`runs/sffinch/quicktime-avkit-mediatoolbox-stage-audit-v2.json`.

The range functions use CoreMedia rational arithmetic and propagate invalid
inputs. Expanded M5 vectors matched M1 exactly, including zero, invalid,
negative-start and rounding cases. `PRIVATE-ABI.md` records the local call-site
layouts; `math-reference.{c,nix}` supplies our independently built observations.

`values.m` adds immutable asset-identity snapshots and timecode metadata.
Existing file identifiers compare device/inode identity, including symlinks;
missing paths have URL identity. CoreFoundation cannot create file-reference
URLs here, so assetURL is a working canonical path URL. It cannot follow later
renames like native file-reference URLs. This limitation is handed to the core
owner rather than hidden. AVTimecode independently stores frame number,
duration, flags and SMPTE fields, matching the native setters; it does not
invent frame-number-to-SMPTE conversion. The constructor accepts four colon-
or semicolon-separated nonnegative components within signed16-bit bounds.
Own large-layout/native-reference tests do not establish every private method.

The remaining 12 imports are the two media-selection-menu functions,
AVAudioLevelIndicatorView, AVCaptureView (+ metaclass), AVPlayerController,
AVPlayerControllerTimeResolver, AVPlayerControlsViewController (+ metaclass),
AVTouchBarCaptureInputSourceItem, AVTouchBarRecordingControlsProvider and
AVVolumeControlSlider. Native runtime method signatures alone do not supply
these classes' behavior. No name-only placeholder is supplied.

MediaToolbox now provides five of its seven reviewed imports, documented in
`../MediaToolbox/README.md`. Both frameworks remain staged. In particular,
Finch AVPlayer here still needs a real H264/AAC output backend before this
AVPlayerView can claim QuickTime playback. The separately qualified movie CLI
and installed asset/capture AVFoundation family do not change that limitation.

Evidence: `runs/sffinch/avkit-stage-v1/`, `media-math-stage-v1/comparison.json`,
`avkit-value-stage-v1/candidate.json`, and the same-byte value comparison in
`avkit-value-stage-v2/` / `m5-avkit-values-v5-results.json`.
