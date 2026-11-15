# SF-AG: Firefox on the M1 ([issue 56](https://github.com/chriskearney/siliconforest/issues/56))

Work in progress, 15 November 2026. The goal is the official, unmodified macOS
Firefox on SiliconForest with hardware WebRender, AVD decode and responsive,
correct presentation at the owner's 150% scale. M0 now has a qualified private-prefix baseline; M1–4 remain incomplete.

## Observed on the real M1

Firefox **160.0** is the current release in Mozilla's
[product-details](https://product-details.mozilla.org/1.0/firefox_versions.json).
Release rather than ESR was selected for current platform APIs and a direct
comparison with SF-U's earlier Firefox 160 QEMU results. The universal macOS
DMG contains an arm64 slice; no Apple application/framework/SDK was acquired.
The download was verified against Mozilla's release
[SHA512SUMS](https://archive.mozilla.org/pub/firefox/releases/160.0/SHA512SUMS).
The archive, pristine extracted application and file manifest stay in ignored
`private/mac-apps/third-party/firefox-160.0/`.

All 110 regular bundle files were copied to a new private M1 directory and
SHA-256 verified **before execution**. The first sandbox-on launch against
installed frameworks exited 255 after approximately 1.03 seconds:
`XPCOMGlueLoad ... XUL: image not loaded; Couldn't load XPCOM.` No browser
window, page, hardware rendering or decoding was observed. No input was
injected and no installed file was replaced. The finite runner recorded its
own PID, wait status and cleanup; raw evidence is in ignored
`runs/firefox/baseline-001/`.

The fresh read-only provider snapshot is `runs/firefox/root-0652/`. It has
177 Mach-O providers; these loaded-image readbacks are suitable for import
inspection, **not** pristine-byte verification (PAC lowering changes readback).
The scan covers all **27** bundle Mach-O images, including dlopened XUL,
GPU/media/plugin helpers, GMP ClearKey, crash helpers and updaters.

The required `sf_gapscan` resolver reports **88 distinct missing non-weak
direct imports** across the bundle. Eleven library paths are absent. This
does not mean all 88 bind at startup: some are lazy imports or updater-only.
The older `cocoa_survey.py` reports 103 because it fails to follow the relative
AudioToolbox re-export. Those 15 AudioUnit imports **do resolve**, through
`AudioToolbox.output-pd`; no AudioToolbox repair follows from that false alarm.
The snapshot initially omitted `/lib`, so transitive closure counts are not
reported as actual installed-system gaps. A subsequent five-provider `/lib`
snapshot closes those dependency gaps. With the private compatibility prefix,
XUL has four remaining direct lazy imports (the CF/CG/printing entries below),
and bundled ANGLE resolves its imports; installer/updater gaps remain separate.

The first private compatibility launch **loads XUL and stays alive for the
20-second bound**, but no Firefox window appears in the inspected compositor
capture. Child starts fail at the installed SBPL compiler (`unsupported
expression`, `unbound variable 'default'`). No sandbox was disabled. This is
the existing SF-U compiler work missing from the installed libSystem, not an
application-binary modification. A private interposer reuses SF-U's compiler
and still calls Sandbox.kext; its enforcement is now observed: invalid input rejects, a valid profile applies
through Sandbox.kext, a forbidden owned file returns EPERM, and /dev/null opens.

Private runs010/011 visibly load **HTTPS example.com over Wi-Fi** and execute
our local fixture. Run012 visibly paints **about:support**, with disk startup
cache enabled and a process-local Standard I/O workaround. Its automation
handshake deadline failed before the late successful reply; the harness now
waits longer and does not send commands after an incomplete handshake. Earlier
local/video captures include a blank content area and soft toolbar text.
Run013 then visibly captures all3 required pages with hardwareWR in the parent,
cacheON/default I/O mapping and sandboxes enabled. That qualified M0 baseline
disables the GPU helper, which also disables WebGL; M1 remains incomplete.
The unmodified bundle has not changed. Run014 restores the default GPU helper and
WebGL **with visible correct triangle output**, by providing the missing
IOSurface broker under a new private service name. All3 required pages are
captured. Foreground rAF wall rate59.998fps over10.017s, median16.66/p9516.68ms;
this is browser callback timing, not a compositor presentation-clock proof.

Run011's structured about:support reports **WebRender**, WebGL1 and WebGL2
**Mesa -- Apple M1 (G13G B1)**. The local WebGL triangle links, has GL error0 and
pixel hash `e4bc0ce1`. It uses native CGL (`webgl.disable-angle=true`) and the
real sandbox compiler v4. A foreground10s rAF measurement records median16.66ms,
p9516.68ms and60fps in browser timestamps. Presentation/wall-clock pacing is
not yet established. Across the browser process family that interval used
13.78CPU seconds /11.059wall seconds (**124.6% of one core**), mostly in the
parent. This is an animation measurement, not idle CPU. CPU native-layer
readback and the point-resolution target remain speed/150% quality gaps.

Parent crashes were caught externally in FreeType smooth glyph raster through
Onyx2D/CoreText. The trace shows simultaneous calls with the same face and
different glyphs. Private patch0200 serializes face access and the global glyph
cache. Three controlled eight-thread cache-miss runs render800 references each,
zero pixel-hash differences,0.107455–0.109128 seconds; installed control aborts
SIGABRT134 after the sequential references. The earlier warm-cache fixture did
not reproduce the bug. Browser30-minute stability is still untested.

The default GPU helper originally spent48.41CPU seconds in50wall seconds; all81
main-thread samples were in NSApplication's headless nextEvent loop. Private
AppKit0201 makes its nil-display branch process/wait on NSRunLoop. In a bounded
0.25wall-second timer probe, control consumes0.249821CPU seconds and candidate
0.005544; **both fire the timer**, so this does not prove universal timer starvation.
Run009's GPU main thread samples all wait in Mach messages. Its parent is then
blocked on Mozilla's startup-cache writer mutex. A32-write71KB I/O probe repeats
normal0.0058–0.00594s versus thread throttle0.6088–1.2075s; process-local Standard
mapping0.045–0.0919s. Run011 instead uses an explicitly labelled missing-cache-
parent diagnostic; run012 uses real cache+Standard. No kernel/core files changed.

Run011's Media capability list includes H264/HEVC/VP9 HWDEC after diagnostic
root AVD prewarm. The first H264 movie submits6 real jobs but AAC setup aborts
before presentation. This failure is preserved. A transparent ASBD/provider trace
then proves FormatList success replaces valid48kHz/stereo defaults with zeros.
Float32 without Packed and an incomplete-FormatList error restore converter
creation; decoding still fails because ESDS is passed as raw ASC. A bounded
DecoderSpecificInfo extractor fixes that third failure. All three fixes were first proven in a
private installed-family AudioToolbox variant, then adopted by the media owner
in commit `b796bf971` and installed with its AUHAL correction at09:33Z.
`audio-toolbox.nix` now imports that owner recipe without applying duplicate patches.
The host parser passes valid/raw/ESDS/truncation/bad-length and20,000 bounded
fuzz cases under ASan/UBSan. That host result is separate from target playback.

**Run video-hw-001 visibly plays all3 original1080p30 clips with their audio
tracks, muted**, using the unchanged release, defaultGPU process, hardwareWR,
real sandboxes, startup cache, private broker and AAC candidatev3:

| Clip | End time | Browser total / dropped | AVD jobs / done delta | 64x36 final video pixels hash |
|---|---:|---:|---:|---|
| H264 + AAC | 10.000s | 300 / 0 | 300 / 300 | 9b80c9e3 |
| HEVC + AAC | 10.000s | 300 / 0 | 300 / 300 | 1a2fdd8 |
| VP9 + Opus | 10.008s | 301 / 1 | 300 / 300 | 4eaeadfe |

All driver error/timeout/reset/DART counters remain0. Each capture visibly shows
the expected bunny frame, and the JS video readback has6912nonblack colour
channels out of6912. Each clip records250rVFC callbacks over10seconds with
median40.04ms and p95 approximately50ms, while presentedFrames advances2→301.
Thus decoded300frames/zero browser drops is **not** a compositor pacing pass.
The deferred-source software control004 completes VP9 with300/0 and the same
final pixel hash, without increasing AVD counters. H264 and HEVC instead stall
at currentTime0/0frames for the14s playback deadline, no media error and no AVD
work. Their initial preloaded-source runs fail the automation handshake; those
failures are preserved. A possible cause is the software VT display-order queue
withholding the first callback while Firefox waits before submitting packet2.
A private paired FFmpeg/VT decode-order candidate now passes native controls
and Firefox005. It retains DPB/reference decoding and opts into decode-order
output with a private FFmpeg flag; hardware/default FFmpeg ordering is unchanged.
Native3codec controls return300 real callbacks per300 packets,300 unique PTS,
all300 NV12 hashes percodec equal to software references. Installed H264/HEVC
controls withhold the first2 callbacks; VP9 already passes. Firefox005 ends
all3 clips with300/0 and the same hardware final hashes, AVD remains0.
Its first clip retains an old harness deadline error after actual end because
cold play startup plus a fixed12s wait exceed14s; new fixtures await ended
under a separate18s deadline. No error has been erased from old results.
Audible output, A/V sync,
non-DRM streaming and zero-copy are still unproven. Earlier single-video-tab
runs remain blank; multi-tab switching exposes the content correctly. A
private NSView hosted-root replacement0204 fixes an independent native
green→blue layer replacement control, but **does not fix Firefox first paint**.
The old native control retains green; the candidate paints exact blue on the M1.

Static75s runs with1 and10 tabs verify actual tab counts, using sandboxes/cache
and the GPU helper. In late33.42/33.86s idle windows, summed browser-family CPU
is0.33/0.51s (**0.99/1.51% of one core**). Summed RSS is2319.86/3487.86MiB
(2.27/3.41GiB); shared pages are counted in multiple processes, so this is not
physical footprint. Browser-reported process→firstPaint is3.260/2.748s; this is
not panel latency. Baseline/Ion and baseline/optimizing Wasm prefs are true;
actual Baseline/Ion frames and initial stock benchmark scores are now recorded below.

A paused-video parent sample spends41/78 samples in layer dispatch/draw,
including GPU readback and CPU image blending. Optional QuartzCore0205 skips
bare commits whose layer contents and IOSurface seeds have not changed. Native
M1 controls:40 empty transactions cause40 delegate calls on the old provider,
zero on the candidate; a changed surface and a changed property each still
cause one. All1/1.5/2x exact checker tests pass. It is **off by default** in the
recipe; browser performance and concurrent-mutation qualification remain open.

The private AVFoundation augmentation preserves the media owner's current
asset implementation and adds the four missing speech/display classes from
pinned PureDarwin source (original dual MPL-2.0/BSD-2-Clause notice/source
retained in the output). M1 probev2: current provider reads2 tracks/10.000000s
and displays1024/1024 exact NV12 red pixels on Apple M1; legacy control reads
0 tracks/NaN and displays0/1024 pixels. Speech truthfully reports no voices.
Probev1's own wrapper install-name load failure remains failed evidence; only
our probe was relinked to the canonical framework name and rehashed forv2.
No Firefox binary was changed. Browser001 then fails XUL load with missing
AVCaptureDevice: the wrapper accidentally selected the asset-only recipe. The
corrected base is the owner’s installed `media/bottle/avfoundation.nix` family,
which also preserves capture discovery. Probev3 verifies that class truthfully
reports no devices, plus the same asset and NV12 controls. Browser002 is pending;
no installed AVFoundation changed.

One paired deferred-source run (HW002/SW005) measures decoder/RDD CPU for
H2640.47/1.34s, HEVC0.53/1.70s, VP90.47/2.04s. Full-family CPU is respectively
19.43/19.21s over14.45/14.35wall seconds,14.42/15.78s over12.28/12.18s,
12.87/13.92s over12.13/12.18s. These include startup/readback/audio and are one
pair, not a spread or a claim of adequate end-to-end speed. The parent still
dominates. Optional0205 does not establish a CPU reduction and stays off.

A separate headless GPU-sharing M1 probe allocates one Asahi render-node GEM
buffer, exports it as a dma-buf and passes it through SCM_RIGHTS to an exec'd
process. Both use Apple M1. The child imports it once and observes the parent's
green and subsequent blue GPU clears, each1024/1024 exact pixels, GLerror0.
Fileport creation/makefd roundtrip also returns0. Readback verifies the pixels;
it does not transport frames. No primary DRM node, display mode or compositor
state is touched. This proves a buffer-sharing building block, **not Firefox
IOSurface GPU backing or zero-copy presentation**.

An opt-in private IOSurface backend now stores BGRA pixels and its metadata in
one Asahi dma-buf. Its opaque Mach port is a fileport; the existing private
broker forwards that right without a protocol change. Allocation requires
`SFFX_IOSURFACE_GPU=1`; default allocation stays CPU-backed. The candidate
validates imported dma-buf handles/size/header and keeps descriptors scoped to
the CF object's lifetime. Native M1 controls: old provider truthfully fails the
GPU-backing requirement; candidate with the flag off preserves exact2048-byte
global sharing and the local negative; flag on proves actual GPU backing for
both local and global allocations, exact2048-byte cross-exec global sharing,
local-ID denial and broker unregister0. A separate GPU/CPU coherence probe
reads1024/1024 expected pixels through the mapped dma-buf after each green/blue
GPU update. Firefox's CGL binding and final window presentation still use their
old copies; the new backend alone is **not zero-copy Firefox**.

Stock Speedometer3.1 completes default ten iterations at**8.39±0.43**, visible
1280×876@1.5. MotionMark1.3 completes its stock defaults at**589.1401**, bounds
566.4179–613.1266, fullscreen-window viewport1707×983@1.5, elapsed326.46s.
These are each one run with the recorded private baseline, not a cross-OS
comparison. A separate Gecko profile records actual `baseline` and `ion`
frames in `fxJitArithmetic`. The first parser fails on omitted trailing null
columns; raw-profile reanalysis with that parser bug fixed finds zero invalid
rows. The old runner failure remains preserved. JetStream001 starts before its
stock test rows exist, throws Node.insertBefore and stalls; it is explicitly
aborted with SIGINT to its PID+start-time-verified owned runner, whose cleanup
runs. It has no score. The next harness waits for the actual Start Test link.

Native synthetic seat controls prove Command-L focuses the URL bar,
Command-T adds a tab, and the owned app-menu socket returns the Firefox menu
tree with its in-window strip shown. Command-W actually closes the entire
window: content and chrome handle lists become empty while the parent lives.
The compositor's WMCLOSE record names that exact owned browser PID. Its Mac
shortcut admission only recognizes bundle IDs catalogued under the four
installed application directories; the private Firefox bundle is absent there.
This is desktop interception, separate from the retained-layer teardown crash.
No physical-input claim or owner application/catalog/default change is made.

The corrected0206 menu geometry passes eight native controls, but Firefox006
with that candidate stalls before Marionette. Its base also differs from the
known0131 control, so browser causality is not isolated. The geometry candidate
is excluded from the installer. TERM7 has adopted proven0200/0201/0202/0203/0204
and0207 into the main framework series; the private recipes now follow those
assembled sources and preserve subsequent owner fixes.

The separate unmuted `audio-hw-002` completes60.1seconds at element volume0.2:
1802 reported video frames,0 drops,1802 AVD jobs/done,0 errors/timeouts. The
installed AudioToolbox output provider returns AudioOutputUnitStart/status0;
235 actual per-process HAL clock samples advance at approximately48kHz. With
HAL consumed frames interpolated at requestVideoFrameCallback wall times,
the median relative offset changes-3.17ms between5–15s and45–55s windows.
Constant latency is unknown; this is not acoustic or physical lip-sync evidence.
The earlier audio001 run hit Marionette's actual30s session timeout; setting
WebDriver timeouts explicitly fixes the harness. That failure is retained.
The same run reports1428 rVFC callbacks, median40.12ms/p9566.66ms, so it does
not establish30fps compositor presentation despite the zero-drop counter.

`quality-002` opens and closes a second ordinary window and a genuine private
window, then downloads53 exact UTF-8 bytes through a content Blob/link and the
browser download stack. SHA256 is
`c247e9ae1688056bdd65ed475bb5f7306e097c46dd3f018484fe22def17ce40b`.
Quality001 inspected a newly created zero-byte file before completion and fails;
the corrected check waits for the expected byte count. The required30-minute
ordinary-site run is in progress; no completed stability claim is made yet.

The first GPU CGL trial traps in IOSurface's shared-header atomic counter on a
write-combined GEM mapping. A candidate uses the documented Asahi WRITEBACK
allocation flag while preserving shared atomics; it is built but not yet
qualified. A later repeated CPU-control child blocks opening the render node;
its PID/start-time-owned family is cleaned, with one pending-kill child left
for the already scheduled kernel reboot. Both failures remain evidence. New
opt-in framebuffer export and whole-window GPU presentation candidates build,
but are excluded from the stability run and daily installer until native pixel,
alpha, reuse and fallback controls qualify them.

## Gaps and ownership

Counts are distinct direct non-weak imports from the initial live scan.
Zero-import dependencies still have to load unless weakly linked.

| Provider | Missing | Current action / owner |
|---|---:|---|
| AuthenticationServices | 8 | absent; privately select existing SF-U implementation, including browser-session manager patch |
| Accessibility | 1 | absent; privately select existing SF-U provider |
| AddressBook | 0 | absent dependency; privately select existing SF-U provider |
| CoreMIDI | 17 | absent; privately select existing no-device SF-U provider |
| IOBluetooth | 1 | absent; privately select existing SF-U provider |
| MediaPlayer | 10 | absent; privately select existing SF-U provider |
| MetalKit | 0 | absent dependency; existing no-symbol compatibility library |
| ScreenCaptureKit | 0 | absent weak dependency; inspect separately from missing strong symbols |
| ServiceManagement | 2 | absent; privileged updater operation remains unsupported |
| Vision | 2 | absent; existing SF-U provider, no OCR claim |
| libcups | 29 | absent; existing SF-U provider, no printing claim |
| AVFoundation | 4 | four classes absent; private current-provider wrapper preserves2tracks/10s asset and1024/1024 NV12 GPU pixels, media owner merge requested |
| CoreFoundation | 1 | CFBundleIsArchitectureLoadable; lazy XUL import, request to TERM5 |
| CoreGraphics | 1 | CGPDFDocumentCreateWithURL; lazy XUL import, request to TERM5 |
| ApplicationServices | 2 | PMPrintSettingsSetValue, PMWorkflowSubmitPDFWithSettings; lazy XUL imports, request to TERM5 |
| Foundation | 2 | NSSocketPort, NSXPCListener in installer/updater helpers; not silently stubbed |
| Security | 8 | mostly installer/updater code-validity/digest imports; unsupported operations stay failures |
| CGL desktop GL | private candidate passes | installed CGL crashes after its `/usr/lib/libEGL.1.dylib` load fails; private matching Asahi CGL passes both core-profile pixel checks |
| Native layers to Wayland | unresolved | current NSView reads CALayerContext back to a CPU window bitmap; real composited subsurfaces requested from DESK5 |
| GPU IOSurface backing | unresolved | existing CGLTexImageIOSurface2D uploads CPU memory and writes it back; cross-process dma-buf storage not implemented |
| Shared FreeType raster/cache | private0200 proven | eight-thread800-render reference pixel check passes; installed control aborts; merge requested TERM5 |
| Headless AppKit run loop | private0201 proven | nil-display timer probe CPU drops; real GPU main sleeps; TERM5 review requested |
| AVD sandbox access | private narrow translation proven warm | explicit IOAccelerator grants exact /dev/pdavd read/write; unrelated service/device remain denied |
| AVD cold firmware load | kernel owner requested | driver firmware read inherits caller sandbox, EPERM masked as ENOENT; root prewarm makes identical sandbox open pass |
| AVD video to compositor | private3codec playback observed | originalAVclips visibly end; each300AVDjobs0errors; AAC3fixes proven, muted; SW/A-V/streaming/zero-copy pending |
| 150% layer target | private0202/0203 proven pixels | exact1024/2304/4096pixels at1/1.5/2x; installed1.5x512mismatches, browser013 all3pages visible |
| Global IOSurface ID broker | private isolated control+browser pass | broker absent on disk; installed child lookup fails; private named broker shares2048bytes exactly, local-surface negative remains denied; browser014 defaultGPU WebGL visible, clean service removal |
| GPU/font sandbox paths | private compiler v4 proven | narrow render-node/Mesa and font-service grants; actual GPU pixels + denied-write/unrelated controls pass,13 host cases |
| Startup-cache throttle | workaround partial | Standard policy real system call forwards status, cache-ON support page visible; full startup/idle measurement pending |
| open HTTP(S) handling | private candidate4/4 pass | default/multiple/literal URL argv and legacy document controls; TERM5 asked to merge, no owner defaults changed |

`firefox/compat.nix` selects the absent providers without selecting core,
AppKit, fonts or the installed decoders for replacement. An explicitly
labelled `legacyAVFoundation=true` **diagnostic** variant supplies the older
SF-U speech-unavailable/BGRA-display classes privately. It does not test the
installed AVFoundation assets API and must not replace canonical AVFoundation.
The installed VideoToolbox/CoreMedia/CoreVideo remain selected.

## Reproduction

Tools and Nix recipes: `integrations/puredarwin/firefox/`. Build on Linux with
the repository's PureDarwin toolchain, never a Mac build or Apple SDK.

```sh
python3 integrations/puredarwin/firefox/acquire.py --7zz /path/to/7zz
python3 integrations/puredarwin/firefox/snapshot.py --out runs/firefox/current-root
python3 integrations/puredarwin/firefox/scan.py \
  --root runs/firefox/current-root \
  --bundle private/mac-apps/third-party/firefox-160.0/unpacked/Firefox/Firefox.app \
  --out runs/firefox/current-scan
nix-build integrations/puredarwin/firefox/compat.nix --no-out-link
nix-build integrations/puredarwin/firefox/probes.nix --no-out-link
nix-build integrations/puredarwin/firefox/cgl.nix --no-out-link
```

Read `runs/m1-coord/log.txt` and the coordination rules before any target use.
Never start within an announced install, reboot, GPU test or idle measurement.
Screen, synthetic input and GPU sequences require `gui.lock`, a before/after
log entry and short holds; no build holds that lock. Do not restart the shared
compositor or signal another agent's processes. `put.py` accepts only new
directories below `/private/var/db/sffx/` and cold-verifies every regular file.
Run `remote-run.py` on the target under that workstation lock, with a new run
directory, the pristine staged bundle and the intended private framework paths.
Its test profile suppresses onboarding without accepting terms for the owner.

## GPU path: hardware WebRender observed, presentation incomplete

Mozilla's release source
[GLContextProviderCGL.mm](https://hg.mozilla.org/releases/mozilla-release/file/FIREFOX_160_0_RELEASE/gfx/gl/GLContextProviderCGL.mm)
requests NSOpenGLContext and first tries the 3.2 core profile. It uses CGL on
macOS; bundling ANGLE does not establish an active ANGLE/Vulkan WebRender path.
[NativeLayerCA.mm](https://hg.mozilla.org/releases/mozilla-release/file/FIREFOX_160_0_RELEASE/gfx/layers/NativeLayerCA.mm)
commits IOSurface-bearing CALayers in transactions and applies the backing
scale. The Apple decoder requests IOSurface-backed pixel buffers.

`gl-api.nix` generates Mesa's own desktop GL dispatch functions from the exact
installed Asahi Mesa source (Omacom b7b03c5), sharing that driver's dispatch
table. This avoids mixing another Mesa version's dispatch offsets or loading
GLX. `cgl.nix` builds the existing CGL against this API and chooses the installed
Asahi EGL instead of `/usr/lib`'s older software EGL. Vulkan/zink autodetection
is disabled by default in this private candidate; explicit requests still work.
`cgl-probe` requires an Apple M1 renderer and checks actual 16x16 RGBA
framebuffer pixels in 3.2 and 4.1 contexts. At 07:06Z on the M1, one request
per profile, the private candidate returned `Apple M1 (G13G B1)`, OpenGL 4.6
Core Profile, framebuffer complete, **256/256 pixels identical** and GL error
zero for both. This proves those CGL rendering requests on hardware. Run011 separately proves
hardware WebRender/WebGL; native-layer zero-copy and frame pacing remain open.
The installed-framework control exited 139 after its EGL load failed. The guarded private v2 also returned a clean CGL error for a deliberately absent EGL
library, then repeated both hardware pixel checks successfully. The failed
installed control remains a failure.

## Sandbox and URL reproduction

`sandbox.nix` builds a process-local interposer for the three sandbox-init APIs,
using SF-U's existing compiler and the real Sandbox.kext enforcement syscall.
It does not replace libSystem or disable Firefox's sandboxes. Its new translation
maps only an unconditional explicit IOAccelerator connection allow to literal
`/dev/pdavd`, renderD device data read/write and read-only Mesa DSOs. Explicit
font-server lookup allows only font/config/cache reads. Thirteen host cases and
real-M1 deny-default controls cover denial, unrelated services/devices, wrong
operations, primary GPU nodes, driver/font writes and unchecked compound forms.
`avd-sandbox-probe` applies a real deny-default policy on target: after root
prewarm its positive case opens AVD, the wrong-service case gets EPERM, an
unrelated device remains denied and the allowed /dev/null control opens.
Before prewarm, the positive case returned ENOENT and the driver's state
reported its firmware read had failed with EPERM. No firmware was acquired,
copied or modified. This dependency is reported to the kernel owner, and a
prewarm is diagnostic rather than a cold-start fix.

`open-url.nix` privately builds the patch to the Terminal owner's open(1).
`url-test.py` uses an isolated HOME, LaunchServices defaults and a windowless
argv recorder. All four checks pass on the M1: HTTPS default routing, multiple
HTTP(S) URLs, literal shell-looking URL bytes and the existing -NSOpen document
argument. The first run omitted its desktop runtime environment and failed;
that failure is preserved. The corrected test changes no owner default or
pinned application. This routing result does not establish Firefox integration.

## Close-tab lifetime and native menu geometry (private candidates)

A synthetic Command-W in `native-ui-003` and `004` crashed the browser parent
with SIGSEGV in `CALayer._setNeedsRender`: a retained layer still held a weak
pointer to a freed `CALayerContext`. Patch0207 recursively detaches the former
root on replacement, invalidation and deallocation, while preserving a tree
already adopted by another context. The native ownership probe fails both old
controls and passes both candidate controls, including layer mutation after the
context is released (`lifetime-menu-probes-v2-results.json`). Firefox005 survives
Command-W with this candidate; its UI harness still fails while restoring a
saved tab handle, so the complete native-shortcut sequence remains unqualified.
Optional transaction filtering0205 is off in the lifetime candidate. These are
private builds, not an assertion that the installed AppKit regressed.

Patch0206 makes NSWindow class frame/content conversions count the native menu
strip, including full-size-content windows. Internal decoration layout uses a
separate border-interior conversion to avoid counting it twice. All eight
native controls pass in `menu-probe-v3-results.json`: menus on/off, normal/full
size, panel and borderless. The old class conversion fails the two menu-enabled
normal/full-size controls. First candidate0206v1 double-counted content height
and remains a failed result. An intermediate v2 staging error loaded the system
AppKit instead of the candidate; its provider path is recorded and it supplies
no candidate evidence. Firefox visual qualification is separate.

The corrected AVFoundation base preserves the current Finch capture-absence
family. Its native v3 probe observes AVCaptureDevice, honestly reports no capture
devices, reads two tracks/10seconds and renders all1024 expected NV12 pixels.
Actual Firefox `video-canonical-002` also plays all three original AV clips to
completion with installed AudioToolbox:300/0frames each, matching earlier
hardware pixel hashes and real AVD work. The earlier asset-only AVFoundation
candidate omitted AVCaptureDevice, prevented XUL loading, and invalidated the
first installer assembly; that assembly remains rejected.

## Milestones still requiring real-M1 evidence

| Milestone | Missing evidence |
|---|---|
| 0 baseline | qualified private-prefix pass014: all3 visible with defaultGPU and WebGL |
| 1 rendering | WebGL software pixel comparison; wall/presentation60fps scrolling/animation and CPU reduction; zero-copy native layers at150% (hardware WR+WebGL+checker observed) |
| 2 media | hardware3codec visible pass above; CPU/pixel SW A/B, audible output and A/V clock, native pacing/zero-copy and non-DRM streaming remain |
| 3 quality | fonts/colour/input/menu/panels/clipboard/download/fullscreen/multiple-window checks; repeat startup/RSS/idle spread (first1/10tab measurements above); JetStream (Speedometer/MotionMark and actual Baseline/Ion observed above); 30-minute crash-free browsing |
| 4 integration | qualified owner installer, actual bundle-icon dock launcher and Firefox default URL handling (windowless dispatch fixture passes) |

Initial Speedometer/MotionMark scores are recorded above. No30-minute crash-free
interval, audible audio/A-V or zero-copy claim is made. Three-codec muted hardware playback is observed as bounded above. DRM/Widevine/EME is outside this assignment. Owner verification remains:
panel sharpness/colours at 150%, physical trackpad feel and shortcuts, and
audible output/lip sync. Logs, synthetic input and renderer captures cannot
establish those physical observations.
