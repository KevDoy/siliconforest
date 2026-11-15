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
The CLI wrapper requires three minutes of owner idle, checks real counters
every50ms and aborts only its newly created child session. No GUI/input/capture.

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

`latency-probe.nix` builds our own silent CLI. Its executable interposes only
AudioDeviceCreateIOProcID, forwards the original IOProc unmodified and exposes
its exact output timestamp to the nested AU render callback in thread-local
storage. It records timestamps in a bounded array, prints after Stop, and
changes no clock/latency property or output device. It tests48k/44.1k client
formats against the existing device rate. A missing HAL callback or exceeded
record bound fails. This is Linux-built/staged only; the M1 GPU freeze at23:26Z
prevents target qualification. No new M1 work is queued during recovery.

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
RDD separate from family and ranks matched threads. Eight synthetic controls
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
