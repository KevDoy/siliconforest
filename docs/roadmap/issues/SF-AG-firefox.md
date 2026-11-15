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
A private paired FFmpeg/VT decode-order candidate is built, not yet qualified.
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
actual tier execution and stock benchmark scores remain unproven.

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
No Firefox binary was changed. Browser qualification with this provider and
canonical AudioToolbox is next; no installed AVFoundation changed.

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

## Milestones still requiring real-M1 evidence

| Milestone | Missing evidence |
|---|---|
| 0 baseline | qualified private-prefix pass014: all3 visible with defaultGPU and WebGL |
| 1 rendering | WebGL software pixel comparison; wall/presentation60fps scrolling/animation and CPU reduction; zero-copy native layers at150% (hardware WR+WebGL+checker observed) |
| 2 media | hardware3codec visible pass above; CPU/pixel SW A/B, audible output and A/V clock, native pacing/zero-copy and non-DRM streaming remain |
| 3 quality | fonts/colour/input/menu/panels/clipboard/download/fullscreen/multiple-window checks; repeat startup/RSS/idle spread (first1/10tab measurements above); JIT, Speedometer/MotionMark/JetStream; 30-minute crash-free browsing |
| 4 integration | qualified owner installer, actual bundle-icon dock launcher and Firefox default URL handling (windowless dispatch fixture passes) |

No benchmark scores,30-minute crash-free interval, audible audio/A-V or zero-copy
claim is made. Three-codec muted hardware playback is observed as bounded above. DRM/Widevine/EME is outside this assignment. Owner verification remains:
panel sharpness/colours at 150%, physical trackpad feel and shortcuts, and
audible output/lip sync. Logs, synthetic input and renderer captures cannot
establish those physical observations.
