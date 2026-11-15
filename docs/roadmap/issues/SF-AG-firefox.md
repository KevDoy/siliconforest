# SF-AG: Firefox on the M1 ([issue 56](https://github.com/chriskearney/siliconforest/issues/56))

Work in progress, 15 November 2026. The goal is the official, unmodified macOS
Firefox on SiliconForest with hardware WebRender, AVD decode and responsive,
correct presentation at the owner's 150% scale. No milestone is complete yet.

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

Subsequent private runs show a Firefox window at 150% and execute the local
fixture (its title reports a measured 27.30fps in the traced software run), but
the captured page area is blank and toolbar text is corrupt. This is **not** a
baseline page pass. Parent crashes were caught externally in FreeType's smooth
glyph raster through Onyx2D/CoreText. The trace shows several simultaneous calls
with the same face and different glyphs. Private patch0200 serializes that face
access and the global glyph cache. A controlled eight-thread cache-miss fixture
renders 800 runs with zero differences from sequential reference pixel hashes
in 0.108095 seconds; the installed-framework control aborts with SIGABRT134.
The earlier warm-cache fixture passed both and did not reproduce the bug.
No performance conclusion follows from single samples; repeated timing and
browser stability remain pending. TERM5 has the proven patch and evidence.

The default GPU helper survives but spins (48.41 CPU seconds in a 50-second
bound). An external sample puts all 81 main-thread samples in NSApplication's
nextEventMatchingMask loop: the denied display connection leaves `_display=nil`,
so the existing call neither waits nor processes the helper's Cocoa run loop.
Private AppKit patch0201 adds the same run-loop processing used by NSDisplay
for this headless case. It is built only, awaiting execution. Hardware rendering
with the GPU process disabled reaches CGL IOSurface texture/write-back activity,
but that log is not about:support or correct presentation evidence.

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
| AVFoundation | 4 | installed facade lacks AVSpeechSynthesisVoice, AVSpeechUtterance, AVSpeechSynthesizer, AVSampleBufferDisplayLayer; request to media owner CODEX |
| CoreFoundation | 1 | CFBundleIsArchitectureLoadable; lazy XUL import, request to TERM5 |
| CoreGraphics | 1 | CGPDFDocumentCreateWithURL; lazy XUL import, request to TERM5 |
| ApplicationServices | 2 | PMPrintSettingsSetValue, PMWorkflowSubmitPDFWithSettings; lazy XUL imports, request to TERM5 |
| Foundation | 2 | NSSocketPort, NSXPCListener in installer/updater helpers; not silently stubbed |
| Security | 8 | mostly installer/updater code-validity/digest imports; unsupported operations stay failures |
| CGL desktop GL | private candidate passes | installed CGL crashes after its `/usr/lib/libEGL.1.dylib` load fails; private matching Asahi CGL passes both core-profile pixel checks |
| Native layers to Wayland | unresolved | current NSView reads CALayerContext back to a CPU window bitmap; real composited subsurfaces requested from DESK5 |
| GPU IOSurface backing | unresolved | existing CGLTexImageIOSurface2D uploads CPU memory and writes it back; cross-process dma-buf storage not implemented |
| Shared FreeType raster/cache | private0200 proven | eight-thread800-render reference pixel check passes; installed control aborts; merge requested TERM5 |
| Headless AppKit run loop | private0201 built only | sampled GPU-helper busy loop; private run-loop fallback awaiting target test |
| AVD sandbox access | private narrow translation proven warm | explicit IOAccelerator grants exact /dev/pdavd read/write; unrelated service/device remain denied |
| AVD cold firmware load | kernel owner requested | driver firmware read inherits caller sandbox, EPERM masked as ENOENT; root prewarm makes identical sandbox open pass |
| AVD video to compositor | unresolved | Firefox decoding, NV12 presentation and A/V sync untested |
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

## GPU path: CGL observed, Firefox rendering still unproven

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
zero for both. This proves those CGL rendering requests on hardware, not
Firefox WebRender, native-layer presentation, throughput or frame pacing.
The installed-framework control exited 139 after its EGL load failed. The guarded private v2 also returned a clean CGL error for a deliberately absent EGL
library, then repeated both hardware pixel checks successfully. The failed
installed control remains a failure.

## Sandbox and URL reproduction

`sandbox.nix` builds a process-local interposer for the three sandbox-init APIs,
using SF-U's existing compiler and the real Sandbox.kext enforcement syscall.
It does not replace libSystem or disable Firefox's sandboxes. Its new translation
maps only an unconditional explicit IOAccelerator connection allow to literal
`/dev/pdavd` data read/write; the seven host compiler cases cover denial,
unrelated service/operation, unfiltered and compound/unchecked forms.
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
| 0 baseline | correct window contents; about:support, local fixture and HTTPS over Wi-Fi |
| 1 rendering | hardware WebRender in about:support; WebGL comparison; 60fps scrolling/animation and CPU; zero-copy native layers at 150% (private CGL hardware pixels now pass) |
| 2 media | H.264/HEVC/VP9 playback through VT and AVD, driver counters, drops, CPU A/B, audio and A/V clock; non-DRM streaming |
| 3 quality | fonts/colour/input/menu/panels/clipboard/download/fullscreen/multiple-window checks; startup/RSS/idle CPU; JIT, Speedometer/MotionMark/JetStream; 30-minute crash-free browsing |
| 4 integration | qualified owner installer, actual bundle-icon dock launcher and Firefox default URL handling (windowless dispatch fixture passes) |

No benchmark scores, crash-free interval, hardware decode or zero-copy claim
is made. DRM/Widevine/EME is outside this assignment. Owner verification remains:
panel sharpness/colours at 150%, physical trackpad feel and shortcuts, and
audible output/lip sync. Logs, synthetic input and renderer captures cannot
establish those physical observations.
