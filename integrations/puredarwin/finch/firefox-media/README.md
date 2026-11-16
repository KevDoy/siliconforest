# Firefox media ownership

Owner priority changed2026-11-15: VLC/QuickTime/AVKit are parked in
`d9673c3c1`. CODEX-FX leads Firefox and owns its one IOSurface/CGL cohort,
launcher/manifest activation and integrated GUI runs. This stream owns the
CV/VT/audio producer and diagnostics. Shared claims/results live in
`runs/m1-coord/firefox-defects.md` and the CODEX/FX/TB request files. No AVE,
pointer/key injection, owner-profile attachment/signals or push.

## Baseline and deployment gap

FX's private20:05Z browser test decodes6001080p H264 pictures and3004K HEVC
pictures with zero reported drops. CGL CPU upload/readback is0; VT still copies
1,866,240,000/3,732,480,000 bytes, one copy per frame. Browser-family CPU is
125.54%/140.05% of one core. rVFC median40.02ms/p9550ms and252/250 callbacks
over10s are not60fps presentation. Isolated M1 CV callbacks run approximately
60Hz; there is no evidence supporting a speculative CV timer change.

**Those are private-cohort numbers.** The owner's installed160-system1 still
selects older surface/CGL providers. `providers.nix` packages corrected CV
and the shared-capture VT producer, wrapped with the existing additive decoder
queries so the installed symbol family is retained. AudioToolbox stays
canonical. FX supplies and qualifies its matching surface/CGL and versioned
support selection before deployment. A build is not an installed result.

Kernel import capability remains0. The [one-install contract](ZERO-COPY.md)
records the missing decoder/GPU pinning, cache ownership and lifetime pieces.
Keep the one-copy fallback and strict rejection until actual shared output,
independent hashes/PTS, GPU completion and browser rate/copy measurements pass.
The earlier149/150 frame-hash failure remains open; a later600-frame exact run
did not reproduce it or prove the uninstalled IRQ fix works on hardware.

## FX-035 cadence boundaries

All five FXQA clips deliver rVFC callbacks around25Hz despite30/60fps input
and zero reported drops. This remains a defect, not a60fps pass. The selected
CoreVideo family retains PD's pthread display link; Finch's dispatch-timer
implementation is excluded by `CoreVideo/pd-displaylink.nix`. An isolated60Hz
callback test does not establish Firefox's actual vsync provider or delivery.

`cadence-trace.c` is a process-local, opt-in diagnostic enabled by
`SF_MEDIA_CADENCE_TRACE=1`. It observes CV display-link callback entry/duration,
VT output callback entry/PTS/duration and CGL IOSurface plane-bind duration.
Arguments, return values, image references and pacing are forwarded unchanged.
It adds no pixel access, GPU wait or per-callback I/O. A4Hz sampler drains a
bounded32,768-event table;64 stable contexts remain until process exit. Do not
unload the image. Missing/interposition-bypassed hooks are unobserved, not free.
This selected CV provider has no `CVDisplayLinkSetOutputHandler` export; the
tracer does not introduce that dependency.

Linux-built M5 controls passed2026-11-16 around01:04Z: baseline and observed
CV programs printed identical properties and216 callbacks/zero forwarding
errors. The trace contains all216 callbacks at119.981Hz on that controller.
An HEVC Main10 admission-only control returned0/session-present with one
observed VT-create event. These establish callback/control transparency on
that Mac, not decoded-output/CGL forwarding, M1 cadence or Firefox performance.
All files were SHA-256 checked before execution in a fresh temporary directory;
each program had a30s timeout, no DYLD overrides and subsequent cleanup.

`prepare-cadence.py` packages those Linux outputs and a snapshot of the shared
Firefox diagnostic sources. `run-cadence.py` requires caller-held gui.lock and
bench, then checks the current coordination gate. Its `control` phase copies
and cold-verifies the packet; `browser` and `profile` use the recorded receipt.
`cadence-browser.py` uses the installed launcher's explicit isolated-profile
observer hook and refuses if that hook is absent. It records owner activity
without waiting, injects no keys/pointer events and runs only its own Firefox
family. The optional Gecko profile is diagnostic, not a performance result.

`analyze-cadence.py` uses each process's actual Mach timebase and accepts an
epoch interval matching the browser fixture. Decoder prefetch/bursts are not
presentation FPS; CGL bind completion is not proof of panel presentation. Eight
synthetic controls cover60/25Hz discrimination, missing hooks/timebases,
duplicate identities, overflow and measurement-window selection. The browser
result option aligns each fixture interval and distinguishes callback frequency
from reported picture-serial jumps; those jumps are not labeled dropped frames.
No25Hz cause
or fix is established until the same browser run observes the boundaries.

`clocked-corevideo.nix` composes TERM9's compositor-clock provider with our
pixel-buffer lifetime addition as ONE full CoreVideo family. It is staged,
not selected by default: the default lifetime recipe resolves to the exact
previous Nix artifact. TERM owns clock qualification and FX owns coherent
browser activation; a clock/export build alone is not a cadence fix.

`prepare-shared-capture.py` creates the strict native kernel-qualification
packet: own600-frame1080p H264 and300-frame4K HEVC fixtures, independent
NV12 hashes/PTS, GPU FD/plane/DATA-offset checks and a picture held64 callbacks.
It uses FX's current NV12 surface/broker with the finalizing-surface lookup fix,
and the single CV/VT producer payload. Each case saves kernel counters around
its own bounded decoder/broker group. `SF_VT_ZERO_COPY=require` rejects copy
fallback. This tests capture correctness, not GPU sampling/fences, presentation
FPS or browser performance; CODEX-TB owns its new-kernel execution gate.

## Audio lifecycle observer

`hal-observer.c` observes API-boundary Start/Stop/Uninitialize/Dispose, exact
provider/status/duration, output callback activity and the same-process HAL
ring. Enable only in our own test process with `SF_FIREFOX_MEDIA_TRACE=1`;
`SF_FIREFOX_MEDIA_TRACE_SECONDS` accepts10..180, default125. Replace the older
start-only observer; **do not stack AudioUnit interposers**. Keep the image
loaded for process lifetime. It never creates output, changes volume/device
settings, controls another process or modifies PCM bytes.

The opt-in output-callback proxy forwards arguments, flags and return values,
counts calls/requested frames/successes/errors using atomics, and restores the
original callback/refCon for GetProperty callers. Counts aggregate all callback
registrations at a unit address, so replacing a callback cannot hide continuing
old activity. Pointer-reuse generations are logged separately. Contexts remain
stable until process exit;128 registrations and16 tracked units are bounded.
Exhaustion falls back to the original API behavior and logs missing tracking.
Callback requests are not audio delivered to speakers.

The4Hz sampler reports public AudioDeviceGetCurrentTime and, when present,
PD's existing `PDAudioSessionIfConnected` ring-v2 protocol. The latter does not
connect a device and remains readable after the public clock returns NotRunning.
The reviewed PD source retains this mapping until process exit; version/layout
and bounds are checked. Unknown/absent extensions are unavailable, not zero
cursors. This is the observed process/device's ring, not a per-unit or acoustic
clock. Another unit or direct HAL user can keep it moving. No AudioUnit is
dereferenced after disposal.

The actual C body passes hosted ASan/UBSan callback forwarding, error-status,
getter transparency, multiple-unit, disposal, address-reuse and capacity tests.
These mocks do not establish native ABI, Mach-O interposition or actual audio.

The first native query control timed out30s during unit setup. Raw
`m5-firefox-audio-query-v1-reference.json` retains TIMEOUT/-9. Removing lazy
AudioUnit symbol lookup inside intercepted calls and using dyld's interposing
image exclusion clears the control: baseline0/0.471s, observed0/0.401s, **all
stdout rows identical**, normal dispose0 with actual Native provider logged.
No exact deadlock stack was captured. Only query mode ran on M5: no initialize,
start or output. Fresh temporary directory/SHA checks/30s/no-DYLD/cleanup rules
were observed. A separate incomplete host packet failed remote SHA preflight
before any program execution and remains recorded as such.

M1 own-silent-unit qualification passed23:07:22–27Z: query and silence exit0,
152 callbacks with no callback errors,56,320 mixed process HAL frames accepted
at48kHz. Two-unit control stops one while the other continues; after the final
stop the ring remains at56,320/running0 over six samples spanning1.274s. Its
1536 queued silent frames remain, so this is not proof of flushing. All
Start/Stop/Uninitialize/Dispose calls succeed through the retained PD provider.
The first stopped sample arrives214ms after Stop at4Hz sampling; that is an
observation bound, not exact stop latency. The earlier post-compositor-restart
attempt refused before either program started because the owner-idle guard
was not satisfied; that refusal is retained.

The14 synthetic classifier controls reject missing counters, in-flight
callbacks, continued completions and cross-device unit-address ambiguity.
The actual receipt is classified as own ring/callback quiescence, with browser
document/acoustic stop explicitly unproved. `audio-probe` queries properties
by default; `--silence` is M1-only and exercises our silent start/stop/restart,
two units, stopping one while the other remains active, uninitialize/dispose
and callback getter transparency. It is not browser tab/window shutdown proof.
Those original runs required three minutes of owner idle. The owner lifted
idle waits at00:25Z on16November: current CLI runs record activity without
waiting or aborting on owner input. Conflicting input affects timing provenance,
not authorization. Only our own child is stopped on its finite timeout. No
GUI/input/capture.

Build `audio-probe.nix`, then:

```
python3 prepare-audio.py --package NIX_OUTPUT --stage NEW_STAGE
flock runs/m1-coord/gui.lock tools/bench.sh run python3 run-audio.py NEW_STAGE
```

Use repository-relative script paths when running from the repo root. The
runner requires a fresh `firefox-audio-lifecycle-stage-*` name, checks the fresh
coordination gate inside locks and hashes all files before any own image runs.
Never read back an executed M1 image as pristine: PAC lowering changes it.
Pair lifecycle/ring evidence with FX's document heartbeat/pagehide/window
enumeration; a reusable content process alone does not prove a live document,
and disappearance of a window/heartbeat alone does not prove audio stopped.

## Output latency audit

The current PD AU drops HAL outputTime and supplies mach_absolute_time instead.
The coreaudiod device backend's readHostTime already models queued frames,
one period and5120 downstream frames; reported device latency is8192frames
(170.67ms at48kHz). These are source values, not a new measured acoustic result.
The M1 query measures8192 device +512 unit +0 stream frames =181.333ms
reported fixed latency at48kHz, with safety offset0. The selected-device AU
reports one buffer as unit latency. Native M5 query
reports unit latency0; presentation-latency40 is unsupported in both baseline
and observed native queries. Do not implement that optional property as success
merely to make it present.

Mozilla's [CoreAudio backend](https://github.com/mozilla/cubeb-coreaudio-rs/blob/076ecf68dbf20087d2fa8e9210a1965e341f66d0/src/backend/mod.rs)
adds device/stream/unit latency to the callback host-time lead. Thus blindly
forwarding PD's full audible-time estimate would double-count some delay.
Measure the callback/ring/device model before changing it. The observer's
historical relative-drift control does not establish constant perceived A/V
offset. No latency or shutdown fix is claimed yet.

## Staged timing and RDD diagnostics

`latency-probe.nix` builds our own silent CLI. Its separately linked diagnostic dylib interposes only
AudioDeviceCreateIOProcID, forwards the original IOProc unmodified and exposes
its exact output timestamp to the nested AU render callback in thread-local
storage. It records timestamps in a bounded array, prints after Stop, and
changes no clock/latency property or output device. It tests48k/44.1k client
formats against the existing device rate. A missing HAL callback or exceeded
record bound fails. The first executable-local variant failed actual M1 instrumentation at23:47Z:
AU callbacks ran, but the HAL interpose was not reached (entry/host/flags0).
That failure remains recorded, with no latency comparison. The hook is now in
a separately linked diagnostic dylib, using the qualified observer's loader
pattern; the corrected version is Linux-built and awaiting target execution.
No target work ran during the23:27–37 recovery hold. The earlier immediate
post-restart attempt refused before execution because three minutes of startup
idle had not elapsed; that refusal is retained separately.

Prepare with `prepare-audio.py --latency --package NIX_OUTPUT --stage NEW_STAGE`
using the same cold hashing/owner guard as the lifecycle probe. Analyze a normal
receipt with `analyze-latency.py`; failed probes cannot be analyzed as success.
The result compares actual HAL/AU clock leads and the cubeb fixed-plus-lead
model. It retains nonfuture/startup timestamp counts and makes no acoustic or
lip-sync claim. Five synthetic clock controls demonstrate the old-now and
naive-full-forwarding model errors; they are not hardware measurements.

`process_cpu.py` supplies capture/compare for CODEX-FX's owned Marionette client.
Pass the verified parent and a fresh map of owned descendant PID to OS start
identity. Capture before/after local MSE or real playback; it restores content
context and never changes native focus or sends input. Gecko reports process
and thread CPU nanoseconds with explicit rdd/gpu/utility types. Compare keeps
RDD separate from family and ranks matched threads. Nine synthetic controls
cover counter resets, zero/unavailable counters, PID reuse and context cleanup.
New/exited/missing-identity processes make family accounting incomplete rather
than disappearing as zero CPU. RPC duration brackets wall-time uncertainty.
Gecko profiler sampling is a separate diagnostic run; its overhead disqualifies
its CPU/frame timing as a performance result. Native RDD counters and real
YouTube profiles remain pending; no source-based cost attribution is claimed.

`codec-capabilities.js` adds only an owned-page callable query. It records
canPlayType, MSE support and MediaCapabilities file/media-source declarations
for H2641080p60, HEVC/VP9 4K30, AV1 1080p30/60, Main10/profile2 PQ, AAC and Opus.
Each query has a3s diagnostic bound. It changes no preference and explicitly
separates supported/smooth/powerEfficient declarations from actual decode,
hardware selection, speed and HDR rendering. Fullscreen still requires its own
qualified real-user activation path; this stream sends no synthetic input.

`make-codec-fixtures.py NEW_DIRECTORY` generates our own Linux test patterns:
10s AV1 1080p30 and VP9 1080p60;2s HEVC Main10/VP9 profile2 720p30 with explicit
BT2020/PQ metadata;3s stereo48k AAC/Opus tones. It validates decoded frame
counts, dimensions and10-bit/PQ metadata, records source commands/probe/SHA256
and software frame-MD5 references. Files stay under ignored runs/, not in git.
The first fixture packet's10-bit files lacked PQ/primaries despite command
flags; that failure is preserved and packetv2 fixes frame metadata explicitly.
These generated patterns do not prove real YouTube speed or HDR visual quality.
No codec matrix result exists on the M1 yet.

The common coordination gate now treats a coordinator `desktop FROZEN` event
as a persistent hold, released only by an actual restoration event from the
named recovery owner CODEX-TB or the coordinator. A quoted marker, another
agent's release or elapsed time cannot clear it. The45 gate controls pass;
this is host policy coverage, not a hardware recovery result.

`codec-admission.nix` extends our existing MIT public-VT demux client only in
admission mode. It uses the real fixture hvcC, requests NV12 or10-bit x420
(P010), and records status/session invariants without feeding samples. The old
8-bit hashing code is unreachable, so it cannot falsely hash P010 as8-bit.
The same Linux-built portable client786db079… ran on M5: NV12/HW and P010/HW/SW
all status0/session present,0.457/0.041/0.049s. This validates the Main10
description and admission test; no frames were decoded. M1 same-byte comparison23:49:46–49Z admits NV12/HW(status0), but rejects
P010/HW and P010/SW with-12906 and null session, both cleanly. No samples were
fed, so Main10 pixel correctness and browser fallback remain unproved. A clean
refusal is never called working10-bit/HDR decode. Prepare
with `--admission --fixture OUR_HEVC_FILE`, using the same fresh cold/coordination gate.

The corrected needed-dylib timestamp probe passed on M1 at23:51:44–46Z:
156 actual callbacks, both48k/44.1k clients, unchanged real-input counters and
no signals/install. HAL lead median191.990ms at48k and197.292ms for44.1k,
while AU callbacks report now and the cubeb source model reports181.333ms.
Median model under-reporting is10.657/15.959ms, maximum21.322ms. Four startup
HAL timestamps are nonfuture and retained as such. One early future timestamp
is149.305ms, where the fixed model instead over-reports32.028ms. These are
software-clock measurements, not a measured DAC/lip-sync offset. The browser's
actual selected cubeb backend/GetPosition still requires integrated evidence.

`clock-provider.nix` builds an opt-in private PD AU variant, applied after the
existing selected-device patch. `SF_AU_NORMALIZE_HAL_CLOCK=1` enables only the
audited speakers/headphones UIDs. Initialization caches device/stream latency
and timebase; the callback subtracts the unchanged fixed device/stream/unit
properties from HAL's already-audible timestamp, and accounts for further
converter callbacks at the client rate. Mutable buffer-size updates publish
new fixed ticks atomically. There are no device queries, allocation or prints
in the render path. Other devices/default behavior remain unchanged. No
CoreAudio/server changes or canonical installation are included.

The fixed-latency floor still over-reports when the HAL predicts a shorter
startup delay. Converter prefetch of a few client samples is not modelled
exactly; do not claim sample-exact or acoustic correction. Nine arithmetic
ASan/UBSan controls and native PD compile pass. The first patch build refused
an ambiguous outputTime anchor; the corrected patch is scoped to outputProc.
Own silent A/B with `--latency --clock-provider NIX_PROVIDER` stages only
AudioUnit/AudioToolbox and retains canonical CoreAudio. This thin private PD
root is for the timing probe; never select it as Firefox's full AudioToolbox.
Firefox integration must merge its child into the full retained Finch family
and prove callback/rate/queue/codec behavior before activation.

Private clock A/B00:01:56–02:01Z passed153 default and160 normalized callbacks:
median cubeb-model under-report10.6566/15.9704ms ->0 at48/44.1k; p95 residual0,
unchanged properties. Startup floor outlier32.026ms remains. Full-family
`clock-family.nix` retains the installed main image byte-for-byte (SHA4bb3b697…)
and135/135 effective exports, with a new normalized PD child. It is built only;
Firefox/queue/codec integration still needs its gate. No canonical replacement
or acoustic proof follows from the silent test.

Current first task is FX-035: all video rVFC intervals are about40ms/25Hz.
No GPU/Firefox workload is permitted before the exact CODEX-TB devfs-fix
restoration marker. Kernel optimization authorization is durable; TB owns its
installs. Source/host work continues during recovery.
