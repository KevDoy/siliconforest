# Finch player source stage

Ports whole `AVPlayer.m` and `AVPlayerItemMediaDataCollector.m` on the existing
asset library, using our own source-derived declarations. The imported source
retains Finch MIT/Apache notices. No Apple SDK headers.

This clock/state/JPEG implementation has no H264 decode/audio output path.
Build and AVKit value-object execution are qualified; media playback is not.
The existing installed asset provider and separate VideoToolbox/AudioToolbox
movie test remain the real media path. Do not install this candidate over them.
