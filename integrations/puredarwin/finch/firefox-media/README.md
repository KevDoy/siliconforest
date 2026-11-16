# Firefox media ownership

Owner priority changed2026-11-15: VLC/QuickTime/AVKit are parked in
`d9673c3c1`. CODEX-FX leads Firefox and owns its one IOSurface/CGL cohort,
launcher/manifest activation and integrated GUI runs. This stream owns the
CV/VT/audio producer and diagnostics. Shared claims/results live in
`runs/m1-coord/firefox-defects.md` and the CODEX/FX/TB request files. No AVE,
pointer/key injection, owner-profile attachment/signals or push.

**Capture correction,16November:** historical media files named
`renderer.ppm`/`youtube-renderer.ppm` came from `--screencapture`, whose CPU
scene omits external GPU window buffers. Their transparent bodies do **not**
prove on-screen blank media. Preserve the original files and failed verdicts
for audit; reclassify their visibility evidence as unqualified. Decoder hashes,
PTS, kernel jobs, CPU and transport counters are independent observations.

The corrected runner keeps a separate `cpu-scene.ppm` and uses the existing
Firefox `renderer_capture.capture` helper for actual GPU readback. It requires
its `source=renderer` log/receipt before accepting the route. This sends only a
capture command under gui.lock; the owner-idle lift applies in a local scope
that restores its previous value. No pointer, key, seat, focus or renderer
setting changes occur. Actual pixels still require inspection; a successful
readback alone is not visible playback or panel cadence proof.

The corrected current-kernel copy/shared comparison completes600 H.2641080p60
and300 HEVC4K30 frames, with zero reported drops in all four cases and identical
final image hashes. All four actual renderer captures were inspected: browser
chrome, page text and the expected video pattern are visible. Decoder pixel
copies fall from one per frame to zero. RDD CPU falls14.745→8.558% and
26.889→16.533% of one core; matched family CPU119.024→115.251% and
151.429→149.472%. This is one unprofiled pair. CGL transport counters exclude
QuartzCore CPU uploads and verification readbacks; this is not a claim that the
whole browser performs no CPU copies. Shared rVFC median remains40.02ms, with
maxima51.64/112ms. Neither those notifications nor inspected end frames prove
the panel presents every frame.

`stage-video-providers.py` stages the complete qualified CV/VT family at a new
inactive versioned runtime prefix, after the notice deadline and fresh bench
gate. All files are hashed on the M1 before execution. It changes no canonical
framework, launcher, manifest, daemon or process. `video-runtime-plan.patch`
hands selection to FX: canonical graphics, network and normalized audio remain
selected; shared allocation uses best effort with the producer's copy fallback.
Strict qualification still uses `SF_VT_ZERO_COPY=require`. The full32-buffer
pool remains the default and invalidation retirement remains off.

After FX selects the family under its guarded install hold, run:

```sh
python3 integrations/puredarwin/finch/firefox-media/run-installed-video.py \
  --stage runs/sffinch/firefox-video-installed-packet-v1
```

The prepared packet contains no private media provider. The actual installed
outer launcher must select the exact family with system graphics and retained
audio/network. The gate requires both frozen clips, zero drops, decoder jobs
and final hashes, zero CGL CPU transport counters, actual browser/RDD CV/VT
including its PD child, and actual renderer receipts. Both GPU images still
require inspection. After measurement and captures it requests ordinary Quit,
requires status0 and no live owned family before cleanup, and collects the
original runtime logs from only its fresh HOME with bounded hash receipts.
The shared runner's original survival-oriented verdict remains false after
intentional Quit; the installed admission reports this explicitly and qualifies
completed playback followed by natural exit separately. FX rolls selection back
if either admission or actual image inspection fails. Source-policy tests and
an inactive cold stage do not establish installed playback.

The installed acceptance subsequently passed all28 checks:600/300 pictures,
zero reported drops and decoder copies, matching final pixels/hardware jobs,
no new decoder faults, actual CV/VT/PD child and retained system graphics/audio.
Both actual GPU captures were inspected. Ordinary Quit returned0 before and
after cleanup, with no live family remaining in1.748s and no cleanup signals.
FX retained the checked rollback and left the family selected. Its separate
installed sample uses RDD7.913/16.327% and matched family114.850/151.338% of
one core; these are not a new matched copy/shared comparison. Pool retirement
remains off. Prepare a fresh packet for any repeat; executed packets are retained.

`youtube_quality.py` requests1080 only after the existing public non-ad video
identity admission. A requested/available quality label is insufficient: actual
video dimensions must reach1920×1080 before measurement. The new
`run-cadence.py <fresh-packet> youtube1080` phase uses the installed outer
launcher and its selected family, without private providers or injected input.
It records actual decoded-frame/media-time rate, drops, owned CPU, media image
paths, actual renderer output and ordinary Quit. `analyze-youtube.py` refuses
lower resolution, non60fps decoding, advertising, incomplete playback, missing
provider/renderer proof, or cleanup-only exit. Actual images still require review.
The quality request changes only this fresh test page; it does not change the
owner's quality preferences, import cookies or bypass consent. Availability or
successful source tests do not establish site playback or panel60fps.

The first real1080p60 execution passed900/0 frames in15.001s, with selected
itag399/AV1/BT709/1920×1080/60fps matching before and after measurement.
Actual GPU output was inspected. RDD CPU is62.793% and matched family186.395%
of one core; MediaCapabilities reports supported/smooth true and powerEfficient
false, with no decoder hardware jobs. Ordinary Quit returns0 with the family
gone in2.743s. This demonstrates real-time software decoding for this interval,
not low CPU, all AV1 content or panel60fps. This first run preceded the
hardware-codec default qualified below.

`mse-installed` and `mse-scroll` use the selected installed family and the same
local H264/AAC files/content/layout. Only the50ms programmatic scrolling timer
differs; it sends no pointer/wheel/key input. `analyze-mse.py` separately checks
the360 pictures, all three resolutions, final pixels, strict shared output,
hardware jobs/faults, CGL transport, own nonzero PCM and at least five seconds
accepted/consumed by the48k HAL ring, then natural Quit. Audio meters add
diagnostic cost. These are local preloaded representation changes, not Internet
DASH adaptation or a trusted-input/performance qualification.

Both installed MSE arms subsequently pass360/0,640→1920→1280 pixel widths,
identical final hashes and360 hardware jobs without new faults. The scrolling
arm records127 programmatic movements. Each own48k HAL ring accepts292864
frames and consumes291328, with nonzero valid PCM and normalized clock;
ordinary Quit returns0 and the families disappear within1.609s. Both actual
GPU images were inspected. Each arm has a283ms initial preview-to-progress
callback gap, with only71ms media-time advance, rather than a scroll-induced
midstream callback stall. This is consistent with audio-clock preroll but does
not prove its cause or panel presentation; the full original maxima remain.

`reuse-media-profile.patch` is a handover against the shared runner, used only
in a frozen diagnostic copy until FX adopts it. `media_profile_admission.py`
accepts only our previous1080 case's exact private directory layout, matching
recorded no-remote command and successful automation/playback, followed by
natural status0 and observed family disappearance. Aliases, owner paths,
incomplete/failed history or cleanup-only exit are refused. It never removes
a profile lock or copies an owner's profile. The ordinary new-launch/owned
Marionette ancestry admission remains required. This permits a subsequent
same-own-profile codec comparison without changing the installed preference.

The `youtubehw` phase disables AV1 only in that admitted own profile. Both
`youtube1080` and `youtubehw` accept `--reuse-media-profile`, `--start-media 1`
and `--compare-pixels`: timing covers the same content window, while a separate
paused128×72 read at10s supplies a small diagnostic sample after measurement.
The hardware admission additionally requires actual AVC/VP9 selection, the
observed false AV1 preference, owned shared-frame counters without copied
bytes, successful hardware jobs and a truthful power-efficiency declaration.
`compare-youtube-codecs.py` checks identical manifest, kernel, actual media/GL
images, profile,1920×1080/60fps/color metadata and content windows, plus lower
RDD/family CPU and diagnostic-sample PSNR. Both actual renderer images still
need inspection. Small samples are not lossless equality or a complete quality
comparison, and one pair never installs a codec preference automatically.

`run-codec-policy.py --stage <fresh-owner-packet>` is the integration owner's
two-case acceptance after selecting its reversible codec policy. Both profiles
are new: the first uses the installed hardware default with no test AV1 pref or
environment override; the second plays only the generated AV1 1080p60 fixture
through `SFFX_AV1=1`, also without a test preference assignment. A scoped
`Popen` observer records absent AV1 keys/prior prefs.js immediately before the
actual installed outer launch. After launch it checks the marked codec block
and live preference. The gates require900 real YouTube hardware frames and600
generated software AV1 frames, zero drops, actual providers/GPU output and
natural status0/family disappearance. Both GPU images still require inspection.
FX owns selection and checked rollback; this command changes no installed file.
The opt-in demonstrates switchback, not default support for AV1-only content.

FX subsequently selected that reversible policy. All30 default and20 opt-in
checks passed, and both actual GPU pictures were inspected. The fresh default
uses VP9 hardware at1920×1080/60fps with900/0 reported drops and shared output;
the explicit AV1 opt-in completes600/0 software frames. Ordinary Quit returns0,
with both owned families gone within2.944s and no cleanup signals. The deliberate
tradeoff is that AV1-only content needs the opt-in. The earlier matched
same-profile AV1→VP9 comparison measures RDD58.819→7.761% and complete family
170.757→125.876% of one core, with identical resolution/rate/color metadata.
Its128×72 paused sample PSNR40.858dB and inspected full GPU images do not prove
lossless or all-scene equivalence. The fresh installed gate is not another
matched CPU measurement. rVFC remains about40ms; panel cadence stays separate.

`audio-session-watch.nix` builds a read-only exact-client metadata observer.
The private packet supplies the original daemon's UUID, `_sClients` offset from
its preferred text address, and original Linux-built artifact hash. The helper
requires that UUID at the actual `/usr/sbin/coreaudiod` dyld image and checks
that the64-client table fits its writable segment. It matches the admitted own
Firefox ring to exactly one server slot by V2 shared-memory object identity and
offset, reads only the48-byte ring header, then releases the client task port
before exit. It retains no audio session/memory-entry right, suspends no process,
writes no process memory and reads no PCM. Only two consecutive all-zero reads
of that matched40-byte slot qualify removal; reuse or partial transitions fail.
The Linux-built own-memory alias self-test passed on the M1 and M5 without
opening the daemon. The M5 verified the binary hash before execution and removed
its temporary directory afterward.

`prepare-audio-death.py --session-watch BINARY --audio-server PRIVATE_SPEC ...`
freezes it with the qualified HAL/Wayland-loss observers. Under GUI→core-shared
→bench, `run-audio-exit.py --stage <fresh-firefox-audio-exit-packet>` uses the
installed outer launcher and selected normalized audio, admits its own fresh
profile/parent, and requires active own48k ring counters before disconnecting
only that client's Wayland FD. The helper must match before that action. The
admission retains the raw clock bridge/removal interval, actual audio providers,
whole-family liveness and cleanup signals. No daemon restart or installed file
change occurs. Exact slot destruction would establish server-client cleanup,
not acoustic silence, output-buffer drain time or every crash mode.

The installed M1 test subsequently passed all12 checks. Its own native log
records a successful Wayland `shutdown`; the exactly matched server slot is
still bound0.378ms later and fully zero by12.193ms, with a second empty sample.
Relative to the controller trigger, removal is bounded55.421–67.236ms; native
clock-bridge drift is0.0012ms. The parent is observed absent within172.439ms and
the full family within400.742ms, with natural status70 and no cleanup signals.
The daemon PID/start identity is unchanged, and both actual normalized audio
providers match the installed selection. This verifies exact server cleanup for
one active-audio connection-loss path. It does not measure acoustic silence,
queued device-audio drain or every abnormal window/termination path.

`audio-state-trace.c` is an opt-in, process-local output meter. It forwards
the original HAL IOProc and records aggregate Float32 power/counters, never
PCM. Render work is bounded and performs no logging/allocation/locking. A
90-second background logger reads an atomic publication; direct read/reset
requires stopped IOProcs. Contexts remain allocated until exit. Enable with
`SF_HAL_PCM_AUDIT=1`; add `SF_HAL_PCM_TRACE=1` for logging. The `SFFX_` aliases
survive the installed launcher's validated private-observer plan carrier.
The logger records its own wall/monotonic pair: Darwin Python and C monotonic
epochs differ here, so raw timestamps cannot be compared directly.

M1 native controls pass on installed and private normalized audio: quarter
power at half amplitude, exact-zero mute, stopped callback counts stable for
250ms, independent concurrent streams and44.1→48k conversion after reinit.
The first rate-change diagnostic failed by reading the retired IOProc;
the corrected test reads the newly registered context. Preserve that failure.
In the private normalized Firefox, all ten HTML/native-output state phases
pass, including mute/pause, a surviving second tab and silence after the last
media tab closes. Half/full power is0.2499133. These are software-boundary
observations, not audible output, glitch-free playback or acoustic sync.
These historical test browsers returned139 on normal Quit, with no family
left before cleanup and no cleanup signals. FX subsequently repaired and
qualified installed SystemConfiguration shutdown; the old failures remain
recorded, not a claim about the current installed shutdown path.

The first installed-outer analysis refused the empty inherited stdout log.
The launcher actually redirects stdout into the NEW test HOME. Recovering
those original logs qualifies both original installed-provider executions:
all ten phases pass, half/full power0.2499733/0.2498880. The empty logs and
initial failed analyses remain; this is corrected analysis of the same runs.
`audio_logs.py` now collects bounded original runtime logs with receipts from
only that run's HOME. The analyzer validates them, or accepts an explicitly
named recovered log with `--native-log`.
The collector's data-only M1 control copies178951 bytes from that original
owned run; its SHA-256 matches the separately recovered log. No browser is
relaunched for this control.

`audio-plan-export.patch` hands the remaining diagnostic-only JSON export
change to FX. FX integrated the selected `SF_AU_NORMALIZE_HAL_CLOCK` enable
separately. Temporary-tree controls execute the current `plan()`/`main()`, apply
only the remaining patch locally, and verify clock policy and observer scoping.
The old clock-gap reproduction skips once that upstream fix is present.
**Direct launches use the full environment, so this JSON omission did not
cause the missing observations and does not block direct-launch activation.**
No shared launcher is changed here. Private-meter `SFFX_HAL_PCM_AUDIT/TRACE`
aliases and its own wall/monotonic pair also support plan consumers.

The native controls also pass with four owned CPU workers active through
every phase, on both installed and private normalized audio. Maximum callback
entry gaps are31.955/33.018ms, respectively; no callback/layout/nonfinite errors,
and the own48k rings accept and consume frames. `--load` requires gui.lock
then bench so another agent's measurements cannot overlap. Workers have an8s
deadline, are forked before opening audio, and cleanup signals only unreaped
own children. This does not measure per-client hardware underruns or prove
glitch-free/acoustic output. No scheduling policy or affinity is changed.

## Per-stream audio controls

`audio-state-probe.nix` builds our M1-only client and an explicitly needed,
default-inactive HAL aggregate meter. `SF_HAL_PCM_AUDIT=1` wraps only validated
packed stereo Float32 output callbacks. It forwards every argument/status,
records power/frame/error counters and never records PCM, mutates output or
changes global volume/device/rate. Sixteen callback contexts remain allocated
until exit. The direct snapshot API requires all registered callbacks stopped
from a non-render thread; PD Stop waits for the active cycle. Reinitialization
creates a fresh IOProc/context. Do not unload the diagnostic image.

`SF_HAL_PCM_TRACE=1` additionally publishes atomic snapshots to a bounded90s
background logger at10Hz. PCM scanning and publication add diagnostic cost;
these runs never qualify performance. Every published word is atomic, with a
bounded sequence check; the worker cannot race a plain C statistics read.
No allocation, logging, waiting or mutex acquisition occurs on the callback.

Native M1 controls on16November passed with the installed and private
clock-normalized AudioToolbox families: half volume gives approximately quarter
power, mute gives exact zero PCM, stopped callbacks remain stable for250ms,
a second stream continues while the first is stopped/muted, and a44.1kHz
client reinitializes into the48kHz output ring. The ring accepted and consumed
frames. The first control failed by reading a retired IOProc after reinit;
that raw failure remains. Corrected controls also pass with background
publication enabled. Startup callback gaps reach166ms and later gaps about34ms:
this is not a glitch-free, acoustic, browser or perceptual-sync qualification.

`prepare-audio-death.py --pcm-observer ...` freezes the new meter and own-page
state helper. `run-audio-browser-state.py` requires gui.lock then bench, checks
the current coordination gate and cold-verifies the packet before execution.
It launches two fresh diagnostic profiles, comparing installed and private
audio, with no injected input. HTML controls cover volume, mute, pause,
restart, concurrent tabs, closing media tabs and normal quit. All diagnostic
browser paths explicitly disable the crash reporter while FX-003 is open.
`analyze-audio-state.py` bridges the independently observed Python/C monotonic
epochs before selecting steady-state native power deltas; missing clocks,
missing meters and invalid PCM observations are refusals. Document state alone
cannot prove output or silence. Acoustic output and per-client server underruns
remain separate measurements.

## Baseline and deployment gap

FX's private20:05Z browser test decodes6001080p H264 pictures and3004K HEVC
pictures with zero reported drops. CGL CPU upload/readback is0; VT still copies
1,866,240,000/3,732,480,000 bytes, one copy per frame. Browser-family CPU is
125.54%/140.05% of one core. rVFC median40.02ms/p9550ms and252/250 callbacks
over10s are not60fps presentation. Isolated M1 CV callbacks run approximately
60Hz; there is no evidence supporting a speculative CV timer change.

**Those are historical private-cohort numbers.** At03:08:52 UTC on16November,
FX selected the qualified NV12 surface/CGL cohort for160-system1. The five
canonical UI providers remain selected; media activation is still pending. `providers.nix` packages corrected CV
and the shared-capture VT producer, wrapped with the existing additive decoder
queries so the installed symbol family is retained. AudioToolbox stays
canonical. FX supplies and qualifies its matching surface/CGL and versioned
support selection before deployment. A build is not an installed result.

CODEX-TB's corrected shared-buffer kernel passed the native strict capture gate:
600 H2641080p and300 HEVC4K frames matched every independent visible-pixel hash
and PTS, retained the first frame across64 later callbacks, and reported zero
pixel-copy bytes. Three pin/import/destroy rounds and decoder context teardown
also passed without errors or quarantine. This qualifies private hardware
capture, not the real Firefox consumer's GPU fence or delivery rate. The
[one-install contract](ZERO-COPY.md) records those remaining boundaries. Keep
the one-copy fallback until the browser's whole path is qualified.
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

The revised tracer's M5 CV control also passed216/216 callbacks at120.006Hz.
A separate public VT decoder control then decoded120 own H264 pictures in
both baseline and observed programs: all frame rows including PTS were
identical, every PTS-sorted visible NV12 hash matched independent Linux output,
and the trace contained120 output callbacks with zero errors. Decode-order
callback hashes initially differ from the presentation-order reference;
sorting by the recorded PTS gives exact matches in both controls. This adds
native VT-output forwarding evidence, still no M1/CGL/browser performance proof.

On the restored M1 devfs kernel, the same tracer passed01:24:51–57 UTC:
baseline/observed output identical,108 callbacks each, zero forwarding errors,
all108 traced at59.984Hz. Nominal/actual refresh properties remain100/6000
and1/60. The Main10 P010 admission control cleanly returned-12906/null with
the same status recorded by the tracer; no samples were submitted.

The first installed-browser boundary trace reproduced the notification limit.
At normal speed,1080p60 H264 produced600 VT callbacks and600 GPU NV12 plane1
bindings at about60Hz, with617 CV callbacks at60.015Hz, while rVFC delivered252
callbacks with median40.02ms.4K30 HEVC produced300 VT callbacks and300 GPU
plane1 bindings at about30Hz,619 CV callbacks at59.614Hz, and250 rVFC callbacks
with median40.04ms. Both reported zero drops and exact matching kernel job/done
deltas; no new errors/timeouts/DART faults, and contexts returned to zero.

The playback-rate discriminator then measured actual rates0.5/1/2 and median
rVFC intervals80.06/40.04/20.02ms. At0.5x/1x, all600 frames were decoded and
bound with zero reported drops. **2x failed:**600 decoded,341 reported drops,
only141 GPU plane1 bindings; rVFCp95 144.62ms/max550.14ms. Preserve this stress
failure rather than interpreting its20ms median as smooth120fps playback.

This rate law matches Gecko's40ms/playbackRate progress updates. At FX's
Breakpad FILE revision2887d9de (not full release-source identity proof),
MDSM position changes reach MediaDecoder's watcher/UpdateLogicalPositionInternal,
then VideoFrameContainer::Invalidate and HTMLVideoElement::Invalidate, which
schedules document video callbacks. RequestVideoFrameCallback queues without
scheduling a document tick. Ordinary image replacement does not invalidate
every frame. Decode and CV are therefore not limited to25Hz in these runs.
The source explicitly advances reported picture serials independently of
visible composition. Do not fix this notification path by changing CV's clock.

These are instrumented callback/binding observations, **not visible playback
or panel FPS**. Actual sampled parent/GPU images used system GL+IOSurface during
the graphics lead's temporary selection trial, which independently displayed
blank content with missing OES dispatch. That trial is being rolled back.
No all-provider or visible-video qualification follows from these traces.
Repeating visual/video qualification with the coherent graphics cohort, fixing
actual callback scheduling, and the busy-input regression remain open.

`prepare-cadence.py` packages those Linux outputs and a snapshot of the shared
Firefox diagnostic sources. `run-cadence.py` requires caller-held gui.lock and
bench, then checks the current coordination gate. Its `control` phase copies
and cold-verifies the packet; `browser` and `profile` use the recorded receipt.
`cadence-browser.py` uses the installed launcher's explicit isolated-profile
observer hook and refuses if that hook is absent. It records owner activity
without waiting, injects no keys/pointer events and runs only its own Firefox
family. The optional Gecko profile is diagnostic, not a performance result.
The fixture selects its own loopback automation port and checks the returned
Firefox PID's ancestry before support/runtime/page commands. It also collects
native image lists from the owned parent/GPU to record the selected providers.
The next packet records process/thread CPU counters before/after each fixture,
with separate RDD accounting, actual OS start identities and explicit unavailable
or changed-process records. Post-measurement media image lists include the RDD;
they verify that private media selected one CoreVideo/VideoToolbox family.
`clocked-providers.nix` composes the existing producer/query wrapper with TERM's
display clock and our415-export CV lifetime provider. `copy`/`shared` phases
use `media-launch.py` only for their explicit owned profile, retain the runtime's
graphics/broker plan and select only the packaged media frameworks. They never
change the installed selector. Boundary interposition is disabled in these CPU
comparisons; native provider sampling remains outside the fixture interval.
`SF_VT_COPY_COUNTS=1` enables the producer's existing exit counters in copy mode
too. It does not change allocation, timing, copying or lifetime policy.
The `rates` phase uses `cadence-rate.js` only in our fixture snapshot, setting
and reporting its requested/actual playback rate. It changes no global browser
preference, decoder clock, display pacing or installed source.

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

`prepare-audio.py --clock-family FULL_FAMILY --latency` selects the full Finch
forwarding family while retaining canonical AudioUnit/CoreAudio. Optional
`--family-tests`/`--fixtures` add WAVE/AAC/ALAC/MP3 decode, file/converter and
WAVE/AAC queue tests; `--lifecycle-package` adds the already-built silent
stop/restart/two-unit observer. Each packet is fresh and hashed before execution.
Passing these CLI tests would not prove browser selection, acoustic sync or
abnormal-window-death cleanup.

Current first task is FX-035: all video rVFC intervals are about40ms/25Hz.
No GPU/Firefox workload is permitted before the exact CODEX-TB devfs-fix
restoration marker. Kernel optimization authorization is durable; TB owns its
installs. Source/host work continues during recovery.

`prepare-audio-death.py` freezes the Linux-built qualified HAL observer and
own Wayland-loss observer with our `audio-death-browser.py`. This separate
installed-launcher test admits only its own random-port/profile parent, starts
our loopback audio document, and shuts down only that client's Wayland FD.
It invokes no seat, input, native menu, compositor restart or owner process.
Family liveness, independent document events and coreaudiod append observations
are recorded against monotonic time for8s before cleanup. Server slot identity
can be ambiguous with other clients; missing/ambiguous drops cannot be declared
audio-stop proof. Source/preparation alone do not qualify abnormal death cleanup.

The full forwarding-family CLI gate subsequently passed14/14 on M1. WAV and
AAC each decoded/queued/completed12000 stereo frames at48k with12 queue callbacks
and no converter error; raw ASC/ESDS float AAC decoded13312 identical frames.
WAVE/AAC/ALAC/MP3 file decode and converter checks passed. The two-unit silent
stop/restart probe saw152 callbacks. Full-family timestamp A/B measured median
under-report10.661/15.956ms ->0 at48/44.1k with unchanged181.333ms fixed properties;
the32.003ms startup over-report remains. Browser selection, acoustic sync and
abnormal-death cleanup do not follow from this CLI result.

A new current-graphics forced-copy browser baseline decoded600 H264 and300 HEVC
frames with zero reported drops, but observed BOTH decoder row copies and CGL
CPU uploads:1.86624/3.73248GB at EACH boundary. Family CPU was114.66/149.59% of a
core; parent83.40/108.87%, RDD9.51/17.44%, GPU13.75/18.59%. Counter identities were
complete with no turnover; rVFC medians stayed40.04ms. These are new-cohort
observations, not improvement over the older different-cohort baseline. Strict
sharing on that older graphics family failed with zero frames/-12910; zero
copied bytes with zero output is not a pass. Its renderer capture failed using
the wrong XDG socket path and is retained as failed. The next fresh packet uses
an explicit complete FX-qualified NV12 graphics override and desktop capture
runtime. `media-launch.py` changes only an in-memory manifest for its own profile;
it never installs a selector or mixes a surface from a different broker/CGL.


The coherent NV12 browser comparison (packet-v10) subsequently decoded all
600 H2641080p and300 HEVC4K pictures with zero reported drops in both modes.
Strict sharing measured zero VT pixel-copy bytes and zero CGL CPU upload or
readback, with exact job/done deltas and no new kernel faults/quarantine. Final
64x36 canvas hashes match forced copy: `b424348` and `88999ae5`. GPU plane
blits still occur; this is zero CPU pixel transport, not zero GPU copies.

| One-core CPU % |1080p60 copy|1080p60 shared|4K30 copy|4K30 shared|
|---|---:|---:|---:|---:|
| Complete family|120.262|120.380|148.098|151.077|
| Parent|87.125|87.582|106.022|111.860|
| RDD|13.803|8.737|25.736|16.205|
| GPU|12.381|18.137|12.337|19.241|

RDD CPU falls about37%; complete-family CPU does not improve. Strict rVFC
medians remain40.02/40.04ms. The actual compositor captures were inspected:
**the Firefox client is transparent/blank in both modes**, despite updating
titles and correct offscreen video pixels. This is a failed visible-playback
gate; no canonical media replacement or owner selector follows. Matching the
installed launcher's Mozilla working directory (v11) and disabling native tile
composition only in the owned profile (v12) both retain the blank client, full
600/300 decode and zero CPU transport. v12's lower family82.978/100.970% is a
diagnostic observation, not a usable preference recommendation. FX's activated
interactive fixture paints with the same graphics cohort; no-input first-show
and the additional staged CV clock remain isolated next controls.

The actual installed-browser connection-loss baseline naturally ends the parent
within149.6ms and the complete owned family within380.1ms, without cleanup
signals. This is distinct from acoustic stop or exact server-slot attribution.

The optional generated-codec matrix freezes Linux hashes and probes each bitstream
using installed media. MediaCapabilities declarations, actual decoder jobs,
CPU, offscreen pixels and visible rendering are kept as separate observations.
Rejected media remains a recorded failure; no codec preferences are altered.

## Verified audio variant and integration request

The full-family AAC/browser test subsequently passed actual selection of both
AudioToolbox and its PD output child, retained canonical AudioUnit/CoreAudio,
and logged `SF_AU_CLOCK normalized=1`. Its last observer sample has244 successful
callbacks/no errors,124419 requested frames and125440 frames written to its own
48k HAL ring. Those are distinct counters, not an acoustic observation. Closing
only its own Wayland connection naturally ended the parent within172.33ms and
its complete family within404.80ms, with no cleanup signals. The server mixed
128000 frames/zero underruns and later closed its output; no client-slot/PID line
was logged, so resource reclamation and acoustic stop remain unqualified.

`audio-runtime-plan.patch` records the exact request for FX's launcher owner.
FX integrated it in commit536ca8013; owner selection remains unchanged. `audio_override` selects a complete versioned
AudioToolbox family and `audio_output_clock=true` enables only the audited PD
clock. Seven namespace/type refusals and two valid plans pass on the host.
The main library remains byte-identical; its retained PD child supplies the
clock correction. `stage-audio-clock.py` copies only that family to a NEW
inactive prefix after the recorded two-minute notice, verifies every file on
M1 before any execution and records an install ledger. It refuses replacement
and partial retries, changes no canonical provider/launcher/daemon and leaves
selection to FX's integrated browser gate. Startup's32ms clock-floor outlier
and perceptual A/V sync are still open.

The private existing-PD-timer control(v13) and canonical-CV plus forced-copy
control(v14) both still show a transparent browser client, despite600/300
complete decoding, zero reported drops and matching final offscreen hashes.
Thus neither the staged CV clock nor its added lifetime implementation is
required to reproduce this no-input visible failure. These controls do not fix
Gecko's40ms notifications or qualify panel playback.

The first v14 collection timed out copying its complete fresh user profile;
that partial archive/failure stays recorded. A separate CLI retrieval recovered
results/captures without rerunning Firefox. Future bounded collections exclude
`*/profile`, retaining that profile and any minidumps on the M1 for a separate
retrieval. Result logs, captures and Gecko diagnostic profiles outside the user
profile are still collected; collection timeout is explicit and does not erase
completed native execution or become a pass.

`client-exit-probe.nix` builds our no-output native resource control. Eighty
fresh exec children each connect an actual HAL ring, verify running0 and call
`_exit` without Close/Stop/Dispose/atexit. All80 succeed on M1 in2.529s; the
reviewed server source has64 slots, so finite session capacity is recycled
across abrupt task death. This does not identify a particular Firefox slot,
prove all80 resources are immediately destroyed, or measure acoustic stop.
Other audio was active; its aggregate server counters are not attributed to
these silent queries. No server/kernel/device setting is changed.

## Installed-runtime high-rate codecs and public YouTube

AV1 1080p60's generated pattern completes600/0 in10.208s with no decoder jobs;
RDD58.67%/family179.22% of one core. VP9 4K30 completes300/0 in10.280s,
RDD27.25%/family151.05%,313 hardware jobs, independently matching300 packets plus13 extra VP9
codec subframes. Both retain40.04ms median video callbacks;
VP9's first gap is144.82ms. These short patterns do not establish all-site speed.
AV1 declarations correctly say not power efficient; no disable/preference follows.

The public YouTube control uses only its own fresh profile and verifies the
requested non-ad video before muted programmatic play, without consent/account
or pointer/key interaction. A separate `youtube-profile` phase adds a Gecko
media profile and must never supply performance qualification. CPU deltas bracket
playback rather than readiness; collection excludes the browser user profile.
The admitted BBB clip advances15.022s in15.024s at1280x720,901frames/0reported
drops. Complete family133.80%,RDD29.38%; hardware jobs do not increase, and the
codec identity was not captured. rVFC median40.04/p9550/max66.70ms. The compositor
capture was inspected and remains transparent inside the Firefox client. This
is a failed visible-playback gate, even with installed media providers. No panel
FPS, perceived A/V sync, or full-owner experience claim follows.

## Private decoder-pool retirement control

`SF_VT_RETIRE_POOL_AT_INVALIDATE=1` is a default-off producer experiment.
Grouped leases allow serialized session invalidation to drop AVFrame references
and release the codec's pool, while retaining the separate IOSurface storage
and the existing consumer usecount/completion contract. Frame destruction runs
outside the list lock; the session's owner reference protects concurrent lease
removal. This must never run during decode/flush or be generalized to decoder
memory that lacks independent external GPU storage.

`prepare-shared-capture.py --hold-final` keeps the final picture alive through
invalidation. `--retire-pool` opts into the producer change; `--gpu-final` also
requires the explicit FX graphics family. It imports both NV12 planes through
that existing CGL provider and checks the independent texture after releasing
the CV picture. The explicit verification readback is a correctness operation,
not a zero-total-copy/performance result. `run-shared-capture.py --codec hevc`
permits one short case. Requested control markers, the actual provider path and
GPU hash are admitted independently; missing controls cannot pass silently.

Real M1/M5 ownership-component outputs agree. Native M1 H264's unchanged control
keeps243792KiB of decoder DART mappings after invalidation until picture release;
the experiment reaches0 while the final picture remains byte-exact/GPU-backed.
Native HEVC's real-GPU case passes all300 reference pictures/PTS/held64 and
held-final checks:843376KiB/one decoder context become0 at invalidation, while
the picture remains valid. After CV release, CGL texture hash remains
`92da379456b1570d817462efdc82c3d4`; provider CPU upload/readback0, GPU blit12441600
bytes. H264's real-GPU texture also matches (`a6db733a4de0e9a25188bac82bc7d4b9`,
3110400 blit bytes), but its combined global kernel gate FAILs because concurrent
jobs and a live context overlap. Both earlier HEVC combined gates similarly
FAIL global-job attribution despite correct pixels. Those failures stay recorded.
Zero decoder DART mappings does not mean every client-owned GPU allocation is
freed. No browser memory saving, visible playback or canonical activation is
qualified. The next gate measures actual browser pause/end/close with retained
GPU contents; owner selection remains unchanged.

## Browser lifetime and stall follow-through

The strict browser end/unload comparison uses the same full32-buffer provider
build and graphics cohort, with retirement enabled only in the candidate.
Ended-video and source-unload snapshots retain the baseline's243792/843376KiB
of global decoder DART mappings; the retirement case reaches0/0 before its first
end snapshot, after detaching15/14 outstanding frame references. Both final
canvas hashes match. These are decoder mappings, not total physical allocations:
RDD ProcInfo memory is32.23/36.51MB versus32.06/33.15MB. No total-memory or idle-CPU
improvement is inferred from these short samples. The collector requires both
owned clips, actual lifetime observations and successful source unloading;
missing observations remain failures.

Both H264 clips report81/83 dropped frames and800.90/837.64ms maximum rVFC gaps;
HEVC reports0/0. All600/300 decoder frames arrive, but that does not make the
first case smooth. The later boundary trace records an H264 VT-output gap of
482.216ms while CV callbacks continue (maximum64.847ms) and CGL plane calls
remain at most4.368ms. New opt-in submit/finish/wait/invalidate timings separate
VT call duration from upstream submission gaps without inspecting or changing
input samples, refcons or output flags. Four Linux-built baseline/observed runs
on M1/M5 retain all120 reference pictures and PTS; presentation-sorted NV12
hashes match the independent Linux reference. Callback order is retained as
observed rather than mistaken for display order. Reference programs run from
hashed temporary directories with no Mac builds or DYLD overrides.

The public YouTube Gecko profile initially failed because profiler control
left Marionette in chrome context; the failed run is retained. The corrected
run restores content context before playback and after stopping the profiler.
It completes actual15.027s/720p60/900frames/2drops, diagnostic only. Markers
identify software AV1 through FFmpegVideoDecoder and bundled libmozavcodec.
CPU-weighted, module-matched stacks identify parent QuartzCore TexImage2D ->
Mesa byte-format conversion; this measured source-owner candidate was handed to
TERM/FX. Selected registered RDD threads do not cover the complete codec worker
CPU, so no complete decoder-cost attribution follows.

A fresh installed-outer public YouTube run after canonical graphics and the
QuartzCore0157 install decodes901/0 in15.019s, but its inspected renderer capture
is still transparent. The no-input first-show reproduction remains with TERM/FX;
no coherent CV/VT activation follows. Whole-browser CPU pixel copies and actual
panel cadence remain outside the VT/CGL transport counters. The runtime's
canonical selector stays unchanged by this stream; an explicit private graphics
comparison changes only a copied manifest and selects that entire cohort.

The submit tracer subsequently locates the repeated H264 gap upstream of the
next call:482.017ms between entries, prior call3.759ms, leaving478.258ms outside
VT; later outside-call gaps475.124/275.730ms also occur during playback. Longest
H264 call after `playing` is5.643ms; callback body0.198ms, asynchronous wait1.75us,
invalidate0.336ms. HEVC's longest playing-time call is19.457ms. Its611ms initial
pool allocation precedes `playing` and is kept separate from steady playback.
This rules out attributing the large observed gaps to time inside our VT call;
it does not identify the upstream scheduling/input/backpressure cause. FX has
the exact timing hand-over. The diagnostic case still reports74/0 drops and
an800.72ms H264 rVFC gap; no speed improvement follows.

`analyze-gecko-media.py --profile RAW.json --symbols XUL.functions.sym
--symbols mozavcodec.functions.sym --output ANALYSIS.json` reproduces the offline
CPU-weighted view. It requires arm64 module headers matching the captured
Breakpad identities and explicit microsecond CPU-delta units. Inclusive weights
count each label once per stack, unresolved native offsets remain unresolved,
and missing CPU deltas stay missing. Wait leaf samples may reflect work before
the stack capture and cannot establish that a wait spins. Selected threads are
not the complete process; diagnostic CPU is not unprofiled performance.

## Local MSE representation control

`make-mse-fixtures.py FRESH_DIRECTORY` generates three two-second fragmented
H264 Main/AAC LC MP4s on Linux:640×360,1920×1080 and1280×720 at60fps, with
120 video packets each. Its manifest records the commands, Linux ffmpeg identity,
stream probes and SHA-256 of each input. Nothing is built on a Mac.
`prepare-cadence.py --mse-fixtures DIRECTORY --mse-pcm-trace DYLIB
--mse-hal-trace DYLIB` freezes these files and `mse-page.html` into a fresh
packet alongside the reviewed CV/VT providers and current FX launch policy.
Run `run-cadence.py PACKET control`, then `run-cadence.py PACKET mse` only
inside gui.lock→bench with the fresh coordination gate.

The page appends three independent muxed init/media files to one MSE
SourceBuffer in sequence mode, then plays its finite local timeline. It records
append ranges, observed video sizes, frame notifications, playback-quality
counters, final diagnostic canvas pixels and DOM geometry. Its audio meters
record own output power and HAL-ring counters. This is an init-segment and
representation-change control, not a DASH/network-adaptation, perceptual A/V
sync, visible playback or presentation-rate qualification.

The native M1 run completed360 decoded frames/0 reported drops and all three
resolution changes, with360 hardware jobs and no new decoder faults. VT copied
bytes and CGL CPU transport upload/readback stayed0. Own48k audio accepted292864
frames and consumed291328 with nonzero output power and no callback errors.
The actual compositor capture was inspected and still shows a transparent
client despite visible/focused DOM and a960×540 video rectangle. That visible
gate FAIL remains; neither staged CV/VT nor lifetime retirement is activated.

`view-state-observer.nix` builds an optional read-only native host diagnostic
and a no-AppKit-load CLI control. The dylib links only libobjc and libSystem;
it never loads AppKit/Foundation or creates NSApplication in helper processes.
With `SF_FIREFOX_VIEW_STATE=1`, existing NSApp/window/view public getters are
observed on the main queue at3/9/15s, capped at8 windows,24 views/window and
depth4. No layer getter, drawing, setters, input or focus changes occur; absent
NSApp and exceptions are explicit observations. Prepare with
`--view-state BUILD_PREFIX`, then run the separate MSE phase with
`--view-state`; this uses the launcher's three-observer limit without cadence
tracing. It is diagnostic only. Linux build and dependency inspection passed;
M1 execution awaits the kernel owner's release, not an owner-idle condition.

The private media wrapper also applies the installed launcher's own per-key
platform and compositor preference helpers to its admitted test profile. Calling
`plan()` alone skipped `main()`'s defaults, so earlier staged runs lacked the
explicit `browser.tabs.inTitlebar=0` and `webgl.disable-angle=true` values.
Existing explicit test choices and unrelated profile bytes are preserved. The
owned browser now queries actual values plus user-value status through its
admitted Marionette session and restores content context afterward. This fixes
a launch-comparison mismatch; whether it fixes the transparent window requires
the same-cohort native A/B, not a host-helper test. Earlier installed-outer
controls were also transparent, so that symptom is not assigned solely to the
private wrapper.

For low-ranked codec identities, add `--roles rdd --top 1000` to the saved
Gecko profile analyzer. The default remains20 labels; changing the output
limit does not change weights or module/CPU-unit validation. In the saved
public YouTube720p60 AV1 run, two registered decoder threads have actual
identity-matched `libdav1d_receive_frame`, `dav1d_send_data` and
`dav1d_get_picture` samples inside bundled libmozavcodec. This confirms dav1d
selection in that execution, beyond merely finding its symbols in the binary.
Unregistered codec workers are still outside that profile. Neither summing
overlapping inclusive weights nor this profiled720p run establishes full codec
CPU or broad1080p speed. The independent unprofiled generated1080p60 pattern
completed600/0 in10.208s at RDD58.67% of one core; visible playback and more
complex real content remain separate gates.

For FX's installed audio-clock activation, the owner gate adds
`--expect-installed-clock` to the owned browser and analyzer. Before launch it
requires the installed `release.json` to select `audio-clock-v1/Frameworks` and
`audio_output_clock=true`; it never overrides that selection. It observes both
actual AudioToolbox main and PD output-child paths in the admitted process.
The host analyzer additionally requires the normalized marker, own48k HAL
accepted/consumed frames, all ten PCM state phases, natural Quit0, no remaining
own family and no cleanup signals. Old139 exits cannot pass this gate.

Prepare a fresh audio-state packet with both qualified native meters and our
AAC fixture, then, inside FX's gui.lock→bench guarded integration hold, run:

```sh
python3 integrations/puredarwin/finch/firefox-media/run-audio-browser-state.py \
  --stage runs/sffinch/firefox-audio-state-browser-packet-owner-v1 --installed-clock
```

That one-profile gate cold-copies the packet, runs the installed outer launcher,
collects only owned logs/evidence, and writes `installed-clock-admission.json`.
An unselected family or missing observations fail rather than silently choosing
the private provider. It is a prepared acceptance gate, not new native evidence
until FX actually selects and executes it. Acoustic sync remains unmeasured.

The corrected M1 capture now verifies `source=renderer` and has been inspected:
full Firefox chrome, page text and the colored final MSE test pattern are
visible. The paired CPU scene omits that body. The run decoded360 frames with2
reported drops; it establishes a visible final GPU frame, not smooth playback
or panel cadence. The native view-getter diagnostic is deferred.

`analyze-video-pair.py PACKET --output SUMMARY.json` summarizes the frozen
600-frame1080p60 H264 and300-frame4K30 HEVC copy/shared cases. It retains raw VT
finalizer rows and deduplicates counters. That log's `frames` field counts only
shared frames: forced-copy mode reports0 there, so the analyzer matches its
observed byte total to the known visible NV12 clip size instead of mistaking it
for zero copies. Missing/ambiguous counters stay unknown. Historical CPU-scene
images never become renderer evidence from their filename or return code.
The summary keeps drops, CPU coverage, transport counters and rVFC notification
intervals separate, and always requires visual review/presentation evidence.
