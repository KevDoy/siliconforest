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
complete and are not reported as actual installed-system gaps.

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
| CGL desktop GL | runtime pending | installed OpenGL uses the older software Mesa; private matching Asahi dispatch/CGL built |
| Native layers to Wayland | unresolved | current NSView reads CALayerContext back to a CPU window bitmap; real composited subsurfaces requested from DESK5 |
| GPU IOSurface backing | unresolved | existing CGLTexImageIOSurface2D uploads CPU memory and writes it back; cross-process dma-buf storage not implemented |
| AVD video to compositor | unresolved | installed VT exists; Firefox decoding, sandbox device access, NV12 presentation and A/V sync untested |

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

## GPU path: built only

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
These are builds, not runtime GPU proof. `cgl-probe` requires an Apple M1
renderer and checks actual 16x16 RGBA framebuffer pixels in 3.2 and 4.1 contexts.

## Milestones still requiring real-M1 evidence

| Milestone | Missing evidence |
|---|---|
| 0 baseline | first browser window; about:support, local fixture and HTTPS over Wi-Fi |
| 1 rendering | actual CGL hardware pixel result; hardware WebRender in about:support; WebGL comparison; 60fps scrolling/animation and CPU; zero-copy native layers at 150% |
| 2 media | H.264/HEVC/VP9 playback through VT and AVD, driver counters, drops, CPU A/B, audio and A/V clock; non-DRM streaming |
| 3 quality | fonts/colour/input/menu/panels/clipboard/download/fullscreen/multiple-window checks; startup/RSS/idle CPU; JIT, Speedometer/MotionMark/JetStream; 30-minute crash-free browsing |
| 4 integration | qualified owner installer, bundle-icon dock launcher and default URL handling |

No benchmark scores, crash-free interval, hardware decode or zero-copy claim
is made. DRM/Widevine/EME is outside this assignment. Owner verification remains:
panel sharpness/colours at 150%, physical trackpad feel and shortcuts, and
audible output/lip sync. Logs, synthetic input and renderer captures cannot
establish those physical observations.
