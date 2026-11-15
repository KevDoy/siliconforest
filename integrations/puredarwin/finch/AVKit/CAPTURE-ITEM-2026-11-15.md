# Capture input item value model

`AVTouchBarCaptureInputSourceItem` is a data item, independently useful without
a touch bar, capture device or recording service. Its implementation stores a
retained title, three Boolean flags and a represented object. The title-only
initializer defaults to not built-in, not the no-audio item, and enabled.
Equality compares title, all flags and represented value; the hash uses the
title. Mutable titles remain mutable through the item, matching the observed
native ownership rather than silently copying them.

Read-only Linux-built runtime metadata established NSObject inheritance and
the typed constructor/getter signatures. It also found that the remaining
`AVVolumeControlSlider` inherits **AVVolumeControlView**, not NSSlider; guessing
that class hierarchy would be wrong. Metadata for the player controller,
controls and capture views is evidence for declarations, not an implementation
or UI/recording capability. No Apple implementation bytes or SDK headers were
obtained. The first metadata build caught a C function-pointer declaration
error; its failure remains recorded. The corrected metadata probe's SHA is
`6b38384c9fdd3ce9936e6e2c22eece93bcf09d5d5f303650b9b613a8ed9e0f35`.

The independent value probe covers constructor flags, equal and unequal titles
and flags, non-item equality, mutable-title retention, represented-object
retention/autorelease/deallocation and equality of distinct equal represented
values. Native getters keep a returned represented object alive through the
autorelease pool: clearing it gives nil immediately, and it is freed after
the pool drains. This does not require or invoke any capture or touch-bar API.

Linux-built portable probe SHA-256:
`76c7418fc159ca4076c450cafb1d486633f7b31b9106576a641b419b6e58a6ff`.
M5 native reference completed with remote SHA verification in a fresh deleted
temporary directory, 30-second bound, no DYLD overrides/build/install and only
program stdout/stderr returned. Raw observations are in
`runs/sffinch/m5-avkit-capture-item-v2-reference.json`.

The new private AVKit artifact preserves all64 previous exports and adds7
class/metaclass/ivar symbols (71total), covering one further reviewed QuickTime
import:8/18. No touch bar, capture device, recording controls, player-controller
family or GUI feature is claimed by this value model. Only the two observed
title constructors and data operations are covered; additional native private
behavior is outside this bounded port. M1 same-byte comparison at20:48:14–15Z
matches every raw stdout row, including represented-value equality and
autorelease lifetime. It uses a coherent private AVFoundation/AVKit prefix,
with all bytes cold-verified before load, under gui.lock and bench. No window,
input, capture hardware or touch-bar service was used. No canonical AVKit is
installed.
