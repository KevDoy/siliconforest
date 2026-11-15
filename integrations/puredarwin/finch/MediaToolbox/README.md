# Staged MediaToolbox utilities

Linux/Nix builds use our own declarations and the installed media families.
Five of QuickTime's seven reviewed imports are provided, with real operations:

* MTCopyLocalizedNameForMediaType and MTCopyLocalizedNameForMediaSubType create
  owned CFStrings using English format names. Unknown nonzero FourCCs retain
  their four characters; zero returns NULL. The public-API matrix includes
  mismatched types, audio/video formats, text and unknown codes.
* FigGetRotationAngleAndFlipsFromCGAffineTransform recognizes orthogonal
  rotations/reflections and writes independent one-byte flip outputs. It
  returns degrees; degenerate/nonorthogonal cases follow observed native
  zero/false behavior. Vectors include scale and floating-point perturbations.
* MTCopyLocalizedStringForVideoDynamicRange copies the observed names for
  values0..8 and returns NULL for unrecognized values.
* MTCopyStringForColorInfo reads actual CoreMedia description extensions and
  emits color-primary/transfer/matrix code points. Missing descriptions return
  NULL; unspecified codes are2. It does not infer color from image pixels.

The two public format-name declarations follow Apple's documentation for
[media types](https://developer.apple.com/documentation/mediatoolbox/mtcopylocalizednameformediatype(_:))
and [subtypes](https://developer.apple.com/documentation/mediatoolbox/mtcopylocalizednameformediasubtype(_:_:)).
Private calling layouts are recorded in `../AVKit/PRIVATE-ABI.md`. Our own
Linux-built programs ran against native M5 and the M1 prefix. Expanded math,
color and format-name vectors matched exactly in
`runs/sffinch/media-math-stage-v1/comparison.json`. These are tested vectors,
not complete private-API conformance or locale coverage. No Apple SDK headers
or native implementation bytes are build inputs.

FigAssetCreateWithURL remains absent: the reviewed application only takes its
address, which does not establish its calling contract. The professional
MediaExtension reader registry also does not exist, so
MTRegisterProfessionalVideoWorkflowFormatReaders has no success placeholder.
This framework is staged only and does not make QuickTime load or play.
