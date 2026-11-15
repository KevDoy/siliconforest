# Staged local-file player

The whole Finch player/item/queue/looper/output model is retained with its
MIT/Apache notices. Our source patch adds SFPlayerMedia for real bounded local
media: AVAssetReader feeds VideoToolbox, decoded NV12 frames are retained in a
bounded CPU store, and ExtAudioFile PCM goes to the existing retained AudioQueue
output. The queue's actual device rate drives the playback clock. Pause, seek
and rate-one playback are implemented; unsupported rates are not simulated.
The original JPEG/ImageIO path remains for assets that do not need this backend.
No Apple SDK declarations are inputs.

`complete.nix` preserves the current browser speech/display/authorization wrapper
and replaces only its loader-relative asset/capture child with the player-bearing
superset. All2501 old effective AVFoundation exports remain,2574total. This is a
PRIVATE PREFIX, not an installed AVFoundation replacement. AVKit's
`player-stage.nix` packages both frameworks and their probes coherently.

M1 evidence: `runs/sffinch/avkit-real-player-stage-v3` cold-hashes the providers
before execution. A source-generated5s H264/AAC movie produces150/150 hardware
pictures with timestamps/NV12 MD5s exactly matching independent Linux FFmpeg.
PDAppleAVD jobs/done increase150, with no new errors/timeouts/DART faults/stale
replies and contexts0 afterward. Audio decodes240000 source frames at48kHz;
pause holds0.468333s, seeking to0.2s returns230400 subsequent frames through
225 real queue callbacks, error0, and playback ends at5.000s. Native M5 and M1
public AVKit value observations match with the same Linux-built binary.

AVPlayerLayer contains real image frames. The view/transport test draws279times
into a CPU bitmap, changes27840 central video pixels from a black reference,
and reaches frame149/ready. These are CPU callbacks and pixels, not279 distinct
presented frames. No window was created; no compositor/panel/acoustic claim.

Limits: synchronous bounded preparation (4096frames,512MiB NV12 and64MiB PCM,
plus source parser/image overhead), local files, mono/stereo PCM output, rate1
and pause/seek. This is not a general streaming/network player, a zero-copy
path, reverse/time-pitch playback, HDR/wide-gamut tone mapping, arbitrary track
transforms, hardware encoding, capture or PiP. CPU RGBA conversion uses the
existing LGPL FFmpeg libraries; notices and relinkable source recipes are kept.
Runtime controls are expected on the main thread. Original failed build
receipts remain under runs/sffinch; no canonical install or AVE work occurred.
