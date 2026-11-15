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

QuickTime's reviewed import inventory: AVPlayerView class and metaclass are
provided (2/18 AVKit imports). Remaining class imports are AVAssetIdentifier,
AVAudioLevelIndicatorView, AVCaptureView (+ metaclass), AVPlayerController,
AVPlayerControllerTimeResolver, AVPlayerControlsViewController (+ metaclass),
AVTimecode, AVTouchBarCaptureInputSourceItem,
AVTouchBarRecordingControlsProvider and AVVolumeControlSlider. Functions missing:
AVAddAudibleMediaSelectionOptionsToMenu, AVAddLegibleMediaSelectionOptionsToMenu,
AVAssetSlowMotionTimeRangeFromNormalizedSlowMotionTimeRange and
AVNormalizedSlowMotionTimeRangeFromAssetSlowMotionTimeRange. Private ABI and
semantics remain unestablished; name-only exports are not supplied.

None of the seven private MediaToolbox imports is implemented: FigAssetCreateWithURL,
FigGetRotationAngleAndFlipsFromCGAffineTransform,
MTCopyLocalizedNameForMediaSubType, MTCopyLocalizedNameForMediaType,
MTCopyLocalizedStringForVideoDynamicRange, MTCopyStringForColorInfo,
MTRegisterProfessionalVideoWorkflowFormatReaders. A real implementation needs
verified signatures/ownership and the corresponding asset/format behavior.

Evidence: `runs/sffinch/avkit-stage-v1/`,
`avkit-network-native-v1-reference.json`,
`quicktime-avkit-mediatoolbox-stage-audit-v1.json`.
