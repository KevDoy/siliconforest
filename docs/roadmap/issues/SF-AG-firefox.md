# SF-AG: Firefox on the M1

[Roadmap issue 56](https://github.com/chriskearney/siliconforest/issues/56) · 15 November 2026 · active continuation: owner system installation authorized; no push by FX.

Official, unmodified Firefox **160.0** now runs on the real M1 with hardware WebRender, WebGL1/2 and hardware H.264/HEVC/VP9 decoding. The earlier review candidate is cleanup-only R7 at `/private/var/db/sffx/release160-r7`. Following the owner's explicit continuation order, Firefox is now installed at `/Applications/Firefox.app`, with versioned support at `/usr/local/lib/siliconforest/firefox/160-system1`. Canonical URL routing now passes. Work continues on presentation timing, video copies and the remaining quality gaps. **Urgent daily-use blocker: downloaded-font text is invisible on real sites.** The earlier99-visit run qualifies loads/process survival only, not legibility. CoreText correction0220 passes denied-write controls and restores CNN/Google Fonts text in reviewed actual-renderer captures; selecting it for the installed owner app and the full ten-site rerun remain pending. **60 fps is not achieved.** The final GPU browsing stability run passes 30 minutes, 99/99 visits across ten sites and zero abnormal content exits. Command-W now passes with the new compositor classification; installed-dock Command-W also passes; pinch delivery remains a gap, so this is not a claim that every daily-use requirement is complete.

## Milestone state

| Milestone | Observed on the M1 | Remaining |
|---|---|---|
| M0 baseline | Mozilla release SHA512 verified; all110 original regular files cold-verified; about:support, local HTML and HTTPS example.com over Wi-Fi; default GPU helper, real sandboxes/cache | Installer/updater/printing gaps are listed below; no unsupported-operation success claims |
| M1 GPU | Hardware WebRender; Mesa Apple M1 WebGL1/2; exact WebGL triangle; CGL3.2/4.1; GPU IOSurface sharing and actual renderer captures at150%; repeated scroll timing and CPU measurements | Latest same-browser passes50.49/50.60/55.68 presented fps; no60fps pass; corrected retained hardware/softwareWR regions: CSS3600/3600 and full WebGL144000/144000 byte-exact; original eight text-edge differences came from an invalid crop (failure retained) |
| M2 media | Cold first-open H264/HEVC/VP9; local1080p hardware and paired software controls; public clear DASH; final R7 each300/0 and AVD+900done/noerrors; unmuted60.1s1802/0 with current canonical HAL | rVFC pacing remains40–50ms; NV12 upload remains; acoustic output and perceptual lip sync require owner observation; HEVC10-bit browser playback not separately qualified |
| M3 quality/speed | Native shortcuts L/T/R/zoom; menus/context menu; precise finger wheel; clipboard/drag; panels/downloads/windows/private window/fullscreen; ICC; Devanagari/emoji/CJK fixes; three stock benchmarks; CPU and finalGPU-path30min stability | Command-W and pinch gaps; physical trackpad feel/momentum and panel sharpness unverified |
| M4 integration | Actual system app, pristine Mozilla payload, own Mozilla dock icon beside Terminal/Activity Monitor, compositor application launch, normal no-Marionette window and same-parent Example Domain URL delivery; uninstall script | Canonical `/usr/bin/open` exact HTTPS routing and installed-dock Command-W pass after TERM c12; late ordinary page-body capture passes; per-app system providers are installed |

Live state is in `runs/m1-coord/status-CODEX-FX.md`. All raw results, captures, minidump/profile directories, build logs and cold receipts remain under ignored `runs/firefox/`; the pristine application remains under ignored `private/`.

## Release, provenance and boundaries

At acquisition, Mozilla's [product-details](https://product-details.mozilla.org/1.0/firefox_versions.json) selected160.0. Release rather than ESR gives current platform APIs and a direct comparison with SF-U's earlier160 QEMU work. The universal macOS DMG contains arm64. Its SHA512 matches Mozilla's [published checksums](https://archive.mozilla.org/pub/firefox/releases/160.0/SHA512SUMS). Source stamp `d84e7a6936554efe26097d9d9b24a0e11330e630`, BuildID20261104181552. Firefox binaries were not patched.

The initial scan covers all27 bundle Mach-O images, including XUL and the GPU, media, GMP, crash and updater helpers. `sf_gapscan` found88 distinct missing strong direct imports and11 absent library paths in the initial177-provider snapshot. The older survey's103 count includes15 AudioUnit imports that resolve through AudioToolbox's relative re-export; those were not real gaps. A five-provider `/lib` addition closes snapshot omissions. The private compatibility selection leaves four lazy XUL CF/CG/printing imports; privileged updater gaps remain separate.

R7 cold-verifies650 regular files and framework/resource symlinks before execution. `release.json` records all Nix outputs, prefix-sensitive recipes and flags. CGL, sandbox policy and fontconfig are rebuilt for the exact prefix. The retained R7 provider map is `runs/firefox/release160-r7-provider-overrides.json`; R7 keeps the qualified UI/font/media providers and selects cleanup-only Quartz GPU7. AppKit UI2 includes0212/0214, Onyx font1 includes0217, Quartz GPU7 includes0216/0219 and excludes0218, CoreText copy1 includes0215, IOSurface GPU2 has the WRITEBACK atomic fix, AVFoundation v3 preserves the current media owner's implementation. The correct IOSurface output is `xld2nnh0sqj1axgsq2sy40l1wxhvcbar-sffx-private-iosurface-session-arm64`; an earlier output is not interchangeable.

Actual parent and GPU dyld image lists are checked, not inferred from environment variables. XUL imports AppKit `Versions/C`; the private provider must expose `C→A`. Earlier GPU003/004, menu006 and installer r3 omitted that alias and silently selected system AppKit. They are not private-AppKit comparisons. The provider check's initial whitespace identity bug leaves005/006 automated failures; separate raw-list reanalysis passes, and later checks pass directly.

All builds use Linux and the repository's open PureDarwin SDK/toolchain. No Apple SDK, font or framework was copied. No firmware bytes were acquired, copied or read back. FX changed only the owner-authorized Firefox application, versioned support, desktop entry, additive dock pins and HTTP(S) handler selection. No kernel, partition, NVRAM, core library, compositor, theme or scale was changed by FX. Executed Mach-O readbacks are PAC-lowered in the page cache and are never treated as pristine hashes. Every transfer uses a fresh private path and verifies before first execution. Only owned PID/start-time-matched process families are cleaned up.

## GPU implementation and measurements

Mozilla's [CGL provider](https://hg.mozilla.org/releases/mozilla-release/file/FIREFOX_160_0_RELEASE/gfx/gl/GLContextProviderCGL.mm) asks for NSOpenGLContext and core3.2; [NativeLayerCA](https://hg.mozilla.org/releases/mozilla-release/file/FIREFOX_160_0_RELEASE/gfx/layers/NativeLayerCA.mm) commits IOSurface-bearing CALayers. Native Mac rendering is CGL/OpenGL. Bundled ANGLE is not evidence of active ANGLE or Vulkan WebRender. These runs use `webgl.disable-angle=true`.

`gl-api.nix` generates dispatch from the matching Asahi Mesa source; CGL uses the installed Asahi EGL instead of the incompatible older `/usr/lib` EGL. The hardware renderer is **Mesa — Apple M1 (G13G B1)**. WebGL triangle hash is `e4bc0ce1`, GLerror0, linked shader. The native CGL probe passes3.2/4.1, and the cross-exec IOSurface probe passes both profiles×2D/RECT,1024 exact pixels per process with no CPU transport.

The private IOSurface backend stores BGRA pixels and metadata in one exportable Asahi dma-buf. A private named broker forwards fileports across processes; local IDs remain unshareable. WRITEBACK mappings preserve shared-header atomics; the original write-combined mapping traps. CGL aliases2D storage or performs GPU rectangle blits. Quartz exports the rendered layer framebuffer; WindowServer composes it with the CPU base and sends an external dma-buf to the existing compositor. CPU fallback remains available. These are GPU composition/export operations, not a claim that video is end-to-end zero-copy.

Ordinary `--screencapture` reconstructs the CPU scene and omits external GPU buffers. Qualification uses the existing inject `capture` command and requires its `source=renderer` log. Verification readbacks are separate from frame transport. Native green/yellow controls have6912/6912 opaque pixels equal to CPU; GPU premultiplied alpha is correct, while the older CPU alpha path has accumulation differences. Native v10 adds a144×96→192×120 resize: cache OFF/ON images match all13824/13824/23040 pixels in the three phases, and CPU fallback matches its complete control. All eight native process controls exit0 with owned cleanup. GPU transport counters report zero CPU uploads/readbacks.

Presented rate counts **unique compositor_ms stamps** inside the fixture's JS wall interval. Multiple callbacks for one flip count once. The compositor delivers these callbacks after DRM flip. Browser rAF remains about60fps and is reported separately; it is not a presentation proof. CPU is process-family TIME delta as percent of one core; children exiting before the final snapshot can be omitted. Fixture scroll is programmatic; focus/wake input is synthetic within a uniquely admitted owned frame.

| Comparison,10s scroll per trial | Presented fps | CPU, %one core | Notes |
|---|---|---|---|
| First CPU transport→GPU transport pair | 11.53→39.79 | 123.69→109.38 | One initial pair |
| Same-provider scalar/uncached OFF008/010 | 37.60/39.00 | 105.21/104.95 | Before equality/lookup controls |
| NEON exact comparison +lookup cache ON009/011 | 50.96/53.48 | 70.64/67.45 | p9533–34ms |
| Revision reuse OFF019/021 | 51.69/44.45 | 73.85/61.34 | Awake, admitted, same boot |
| Revision reuse ON020/022 | 55.49/53.78 | 63.14/63.56 | p9518/34ms; CPU variable |
| CGL export-cache OFF023/025 | 48.87/53.79 | 63.68/61.36 | Same providers, same boot |
| CGL export-cache ON024/026 | 53.69/53.38 | 62.45/62.15 | No repeatable independent FPS gain |
| R6 same-browser repeats001 | 50.49/50.60/55.68 | 65.80/86.10/56.16 | All correct pixels; warmup alone does not resolve60fps |

Exact NEON equality checks every byte. Native720 boundary/alignment/change controls have0errors;16×8MiB scans take about.1305s scalar versus.00865s vector. Cached provider lookups remove per-frame dlopen. Revision reuse tracks every layer/content identity plus local contents seed and shared IOSurface seed; unknown content disables reuse, and mutation/timer paths invalidate. GPU synchronization stays intact.

The opt-in CGL export cache keeps two IOSurface texture/FBO slots per context, evicts resized surfaces and clears retained resources on context teardown. It retains the same blit, glFinish and seed publication. It is enabled in private R6 and R7. Native v10 records two hits/four imports/two evictions over resize. Actual integration006 records136hits/six imports; early-scroll profile002 records2103hits/six imports. Nineteen repeated export-image-import samples in the old early profile disappear in the new corresponding path. Remaining GPU synchronization and layer rendering still cost substantial time. This proves removed work and runtime correctness, not a60fps or independent FPS improvement.

The content early-scroll profile waits in condition variables in416/431 samples. Parent profiles show layer rendering/export, GL completion waits and base comparisons. Profiled runs are diagnostic only and excluded from timing qualification. Runs014 and cache-OFF015 both lose presentation while the display is asleep; neither is a valid cache comparison, and no cache-caused freeze is established. `presentation_seat.py` now verifies unique title/frame/focus and a live renderer without changing idle/power settings.

R6 completes1800.440s GPU browsing:99/99loaded visits,95normal/0abnormal/0unclassified content exits, sameGPU/liveparent/no minidumps, released16:15:53UTC. The later cleanup-only QC7 uses the same immutable source snapshot. A retained source diff confirms that its only CALayerContext change is deallocation: invalidate first, clear/release owned renderer/root/texture-id array, destroy owned CGL context/pixel format. Rendering methods are identical. Native old/new release counters and pixel/resize/retained-tree controls exercise that change; R7 actual integration007, quality003 normal/private window-close and53-byte exact download, videoGPU004 allthree300/0/+900AVD/noerrors and audioGPU004 unmuted60.1s1802/0 allpass. The R6 thirty-minute result is not relabelled as an exact R7 run.

Actual R7 reference001 confirms hardware WebRender versus **WebRender (Software)** independently in about:support. Its original screen-coordinate crop was misplaced: eight text-edge pixels exceeded the strict one-level tolerance. The original `complete:false` and exit1 remain preserved; no threshold was relaxed. An independent audit of the same retained source=renderer captures verifies all four opaque canvas borders, then compares the complete480×300 interior:144000/144000 pixels byte-exact, maximum channel error0. The complete60×60 CSS rectangle likewise matches3600/3600, maximum0. Both WebGL buffers have hash `e4bc0ce1` and GLerror0. Keep hardware rendering; the old differences do not establish a WebGL blend/shader defect. A marker-calibrated rerun is a separate regression control. HW/SW presented rates are53.27/50.52fps at63.19/66.47%onecore. Actual rAF callback wall median/p95/max is17/19/21ms hardware,17/20/21ms software, with no hardware callback gap above25ms. Thus the observed presentation shortfall is not explained by long content callback stalls in that interval.

The same support snapshot reports display metadata2561×1601@0Hz while actual renderer captures and native fullscreen bounds are2560×1600. This off-by-one/refresh metadata discrepancy is reported to the framework owner; its cause has not been isolated.

## Hardware video and audio

Final `video-gpu-004` uses the actual R7 outer launcher and installed VideoToolbox/CoreMedia/CoreVideo/AudioToolbox, with private AVFoundation augmentation and real sandbox policy. The local fixtures are1080p30,8-bit; no separate browser10-bit claim is made.

| Clip | End | Frames/dropped | AVD done delta | Final64×36 pixel hash |
|---|---:|---:|---:|---|
| H264 +AAC | 10.000s | 300/0 | 300 | `9b80c9e3` |
| HEVC +AAC | 10.000s | 300/0 | 300 | `1a2fdd8` |
| VP9 +Opus | 10.008s | 300/0 | 300 | `4eaeadfe` |

AVD totals increase900done; errors/timeouts/resets/DART faults remain0. The cold `video-cold-001` begins with driver idle/opens0, no root prewarm, and also ends each codec300/0 with900done/noerrors. The sandbox compiler grants only explicit IOAccelerator access to exact AVD/render-node paths and read-only Mesa DSOs; an optional literal firmware READ_ALL grant permits cold loading through the existing driver. Wrong-service, unrelated-device, firmware-write and font-write controls remain denied. No firmware content is read back or copied.

Browser Media capability data reports H264/HEVC/VP9 HWDEC. Decoded NV12 still uploads for composition; no end-to-end zero-copy claim is made. Final rVFC counts are246/250/250 in10s, median40.04/p9550ms. Zero browser drops and complete decoding therefore do not establish30fps presentation.

A paired private FFmpeg/VideoToolbox software callback fix retains decoding/reference order while allowing decode-order delivery when Firefox requests no temporal processing. Native controls return all900callbacks/PTS/NV12 hashes matching references; old H264/HEVC controls withhold their first two callbacks. Actual Firefox software005 ends each codec300/0 with matching final hashes and AVD0. Decoder/RDD CPU in one HW/SW pair is H264 .47/1.34s, HEVC .53/1.70s, VP9 .47/2.04s. Full-family CPU includes startup/readback/audio and is not an isolated decoder benchmark. This software fix remains a separate candidate handed to the media owner; final R7 uses the canonical hardware path.

Public `streaming-dash-002` uses the official dash.js4.7.4 reference player with clear Envivio1080p H264/AAC-LC:375/0,14.974media/15.004wall seconds, AVDdone+375/noerrors. Driver jobs+1432 include command accounting and are not375decoded-frame jobs. It is muted. Widevine/EME/DRM is outside scope; there is no YouTube or protected-streaming claim.

Final `audio-gpu-004` launches the original executable with exactly the R7 providers plus a transparent clock observer. Canonical AudioToolbox.output-pd returns AudioOutputUnitStart/property status0. Unmuted volume.2 playback ends60.1s,1802/0, with235 actual same-process HAL consumed-frame samples at about48kHz. Interpolating at JS callback wall times, median video-minus-HAL offset changes **-2.601ms** from media1..10s to50..59s. Constant output latency is unknown; this is not acoustic or perceptual A/V proof. The earlier audio-hw-002's separately defined5..15/45..55s windows give-3.17ms; those older results are preserved, not silently redefined.

## UI, fonts and speed

All input below is synthetic and admitted to the owned uniquely titled Firefox window before delivery through the compositor seat/AppKit.

| Control | Evidence and limit |
|---|---|
| Command-L/T/R, zoom+/-/0 | Native URL focus, tab count, actual reload navigation entry and zoom values pass; Command-W separately closes the whole private window while its parent survives |
| Menus/context menu | Owned app-menu socket supplies menu tree; native0212 appearance inheritance/retention/clear/destruction controls pass; browser opens popup and Escape dismisses it |
| Precise finger scroll |0210+WindowServer bridge delivers37pixel input→DOMdeltaMode0/deltaY37/scrollTop100→137; native fractional.5/-1.25, allfour phases and modifiers pass; no fake momentum |
| Clipboard | Only after an observed empty Wayland generation, UTF8 copy→native read, native write→trusted paste pass; conditional cleanup clears only its own generation; no existing owner clipboard replaced |
| Drag/drop |0214 passes destination pasteboard to item provider; native old/new item-style and board-style controls distinguish bug; actual017 records trusted start1/enter1/over9/drop1/end1, matching text/copy; overall nativeUI remains false because Command-W fails |
| Open/Save | Native panels002 open and save39 exact bytes; first unselected-open failure retained |
| Download/windows | Final R7 quality003 downloads53 exact UTF8 bytes; SHA256 `c247e9ae1688056bdd65ed475bb5f7306e097c46dd3f018484fe22def17ce40b`; second normal window and genuine private window open/close |
| Fullscreen video | fullscreen002 records251/0 over8.37s, native2560×1600 bounds and successful exit |
| ICC | Controlled sRGB-display profile: colour-management OFF linear patch128, ON188, sRGB patch128, exact alpha255; no physical display calibration claim |
| Tab strip | Correct160 preference is integer `browser.tabs.inTitlebar=0`; unused `browser.tabs.drawInTitlebar=false` did not help. Child/content heights960points and frame982; full strip captured |
| Devanagari | Pinned official Noto Sans Devanagari Regular/OFL1.1 plus private fontconfig; actual008 and R5/R6/R7 glyph captures show text instead of tofu. Private read-only font sandbox controls pass; caches stay under private prefix |
| Emoji/font copies |0215 preserves actual graphics face for rendering-only descriptor copies, including unregistered memory font; old glyph42→0/Inter, new42→42 with size41 and matrix1.25×.75. Actual010 and R5/R6/R7 show smiley/tree/rocket and use private CoreText in parent/GPU |
| Collection face |0217 is in Onyx, the active resolver; propagates fontconfig FC_INDEX to FreeType. Native font-controls-v6 requested/copied/rendered CJKkr allkr; old alljp. AppKit-only experiment had no effect and is not promoted |
| Pinch | Selected AppKit magnification getter unconditionally returns0; no alternate getter found. Real value/phase storage and compositor delivery requested from TERM/DESK. This is an implementation gap, not merely an owner observation |

The font-copy microbenchmark uses256 copies, three trials: old5.987/6.038/6.014ms, new2.006/2.020/2.010ms, while correcting identity. Constructor-only font registration reached CoreText's separate database but not AppKit's/Onyx's; it remains diagnostic and is not the daily font mechanism. Font config, not copied Apple fonts, supplies the common resolver view.

Final R7 startup/idle runs last75s, using static local HTML with no script, animation, media or network requests. Every requested tab's loaded URL and body marker are verified.

| Tabs | Late idle window | Family CPU, %one core | Summed RSS | Process→firstPaint | Launcher exec→firstPaint |
|---:|---:|---:|---:|---:|---:|
| 1 |32.593s | **0.89%** (0.29 CPU s) |2216.70 MiB |2.930s |**3.082s** |
| 10 |32.928s | **1.43%** (0.47 CPU s) |2268.86 MiB |2.888s |**3.044s** |

RSS counts shared pages repeatedly and is not physical footprint. CPU uses owned-family TIME deltas and can omit children exiting before the final snapshot. FirstPaint is a browser timestamp, not observed panel latency. These are single runs with a retained private font cache, fresh test profiles and actual hardware WebRender; no cold-cache startup distribution is claimed.

Earlier75s data-URL runs produced about:blank initial snapshots. They remain blank-tab baselines: 1/10 tabs used0.99/1.51%onecore,2319.86/3487.86MiB summedRSS, process→firstPaint3.260/2.748s. In R6, the first launch with a new private font cache reported6.906s; subsequent profile/video/audio launches reported3.106/2.922/3.055s. Those older results are not relabelled as qualified local-page startup.

| Stock benchmark, one completed baseline run each | Score | Method |
|---|---:|---|
| Speedometer3.1 |8.39±.43 | Default ten iterations,1280×876 CSS viewport@1.5 |
| MotionMark1.3 |589.1401 [566.4179,613.1266] | Stock defaults,1707×983@1.5,326.46s |
| JetStream2.2 |94.079 | Stock workload,1280×876@1.5,230.54s; no between-run spread |

The baseline uses the earlier CPU native-layer transport. Final R7 results:

| Stock benchmark | Final score | Method |
|---|---:|---|
| Speedometer 3.1 | **22.6 ± 1.5** | Default ten iterations; 1280×876 CSS viewport at 1.5×; 57.577s; overall pass |
| MotionMark 1.3 | **698.3992 [563.4371, 765.4757]** | Stock defaults; 1707×961 at 1.5×; 317.512s; overall pass |
| JetStream 2.2 | **173.64** | Stock workload; 1280×876 at 1.5×; 114.514s; overall pass |

These are stack-level comparisons, with one qualified final run per benchmark and no between-run spread or attribution to one patch. MotionMark's viewport differs from the baseline. MotionMark repeat002 uses ten synthetic pointer-only wake pulses, about30s apart, after fresh own-parent/profile/window/top-stack/frame checks. No clicks, benchmark workload changes or desktop idle-setting changes occur. Its final actual renderer capture passes. First run001 completed587.1433[386.2575,705.9001] but failed final capture with NoCurrentScene, consistent with display idle; its overall failure is retained. A separate Gecko profile contains actual Baseline and Ion frames in `fxJitArithmetic`; Baseline/Ion and baseline/optimizing Wasm prefs are on.

Earlier `stability-001` completes1800.45s,99/99 loaded visits across ten ordinary sites,95 normal content shutdowns,0 abnormal and0 unclassified, same GPU helper/live parent. It uses CPU native-layer presentation and0207 lifetime fix. Final `stability-gpu-001` uses R6 GPU presentation, observes `ipc:content-shutdown` abnormal flags and GPU identities, and checks each loaded document. Synthetic per-visit owned focus keeps the existing display awake; every tenth visit verifies a renderer capture. No power setting is changed. Final result:1800.440s,99/99 loaded visits,95 normal/0abnormal/0unclassified content exits, sameGPU, liveparent, no minidumps and overall measurement pass. The slot released16:15:53UTC.

## Gap register and ownership

Counts below are the original distinct strong-import scan findings, not a count of runtime failures or claims that unused APIs work.

| Gap/provider | Fix, evidence or owner |
|---|---|
| AuthenticationServices8, Accessibility1, AddressBook dependency | Private compatibility selection from existing SF-U; browser login/accessibility beyond tested flows unqualified |
| CoreMIDI17, IOBluetooth1, MediaPlayer10 | Existing private no-device providers; no hardware-device functionality claimed |
| MetalKit dependency, weak ScreenCaptureKit dependency | Compatibility dependencies only; no Metal device or screen-capture functionality claim |
| ServiceManagement2, Vision2, libcups29 | Privileged update/OCR/printing unsupported; no success stubs represented as working features |
| AVFoundation4 classes | Private current-owner augmentation preserves asset/capture APIs and adds pinned PureDarwin speech/display classes; honest no voices/devices; asset2tracks/10s/NV12red1024pixels and actual Firefox pass; media owner request |
| AVFoundation authorization | Honest Restricted status and one asynchronous NO; native/Firefox support pass, commit7f5fdaa81; no capture permission success invented |
| CFBundleIsArchitectureLoadable1; CGPDFDocumentCreateWithURL1; printing2 | Four remaining lazy XUL imports; requests to TERM; not exercised as successful features |
| NSSocketPort/NSXPCListener2; Security8 | Installer/updater helper gaps; not silently implemented as successes |
| CGL/EGL/GL dispatch | FX private matching Asahi CGL/dispatch; installed control139 retained; hardware contexts pass |
| IOSurface broker/GPU backing | FX private named broker and WRITEBACK dma-buf backend; cross-exec bytes/pixels, local denial and cleanup pass; no canonical broker install |
| GPU native-layer presentation | Private0208/0209 plus CGL/IOSurface/WindowServer, actual renderer and fallback controls pass; NV12 copy and pacing gaps remain |
| GPU base compare/lookups/reuse |0211/0213/0216 plus vector WindowServer comparison, paired measurements above; all opt-in in private provider recipes |
| GPU export allocation/import churn | CGL0004 native/actual hit/pixel controls pass; R6/R7 enabled; no repeatable independent FPS gain |
| Refresh notification batching |0218 native pixels pass but actual Firefox OFF42.97/53.00fps versus ON38.66/42.14; rejected for pacing. Excluded from R7 at compile time and from defaults; experimental recipe/runtime flags retained for reproduction |
| Context resource lifetime |0219 native v1/v2 old control creates12/destroys0/roots0 (exit1), fixed12/12/12(exit0), all12288greenpixels correct and12foreigncontexts preserved. Cleanup-only QC7 additionally passes all8 GPU/native controls, resize and retained-tree new-window adoption. Final R7 browser window lifecycle/media qualification passes; TERM handover sent |
| Shared FreeType raster/cache race |0200 eight-thread800-reference controls pass; old aborts; adopted by TERM7 |
| Headless AppKit spin |0201 run-loop wait/timer controls pass, CPU.249821→.005544s per.25wall; adopted by TERM7 |
|150% backing target |0202/0203 exact1024/2304/4096pixels at1/1.5/2x; old1.5x512mismatches; adopted by TERM7 |
| Hosted root replacement |0204 nativegreen→blue fixed; did not independently fix browser first paint; adopted by TERM7 |
| Retained weak layer context |0207 detach former root on replacement/invalidate/dealloc; old controls and browser crash; new lifetime/browser controls pass; adopted by TERM7 |
| Empty transaction filtering |0205 native controls pass but browser reduction/concurrency not established; OFF and excluded from daily defaults |
| Menu geometry |0206 native8controls pass after v1 correction; browser causality not isolated; excluded. Correct Firefox titlebar preference separately removes tab clipping |
| NSMenu appearance |0212 native lifecycle and actual popup/Escape pass; private UI2; TERM request |
| Precise wheel/phase |0210 private AppKit+WindowServer and native/fingerDOM controls pass; TERM request; physical momentum unverified |
| Pasteboard provider destination |0214 native old/new provider styles and actual trusted drop pass; TERM request |
| Font fallback/config | Private OFL Devanagari fontconfig plus exact-prefix font sandbox read; actual glyphs and four allow/deny controls pass; no owner font/config writes |
| CoreText copy face |0215 native identity/glyph/matrix/timings plus actual010/R5/R6/R7 pass; TERM request |
| Onyx collection index |0217 nativev6 plus actualR5/R6/R7 pass; TERM request; AppKit-only attempt ineffective |
| Command-W | Desktop WMCLOSE for uncatalogued private bundle while parent survives; DESK request; owner catalog activation is not presumed to fix it until tested |
| Pinch/magnification | Selected getter0, real gesture route/value storage missing in audited source; TERM/DESK request; not a qualified feature |
| AVD sandbox/cold firmware | FX bounded compiler translation; explicit capability only, native positive/negative/cold and actual cold900done pass; no firmware readback/prewarm |
| AAC FormatList/flags/ESDS | Proved privately, adopted by media owner b796bf971 and installed09:33Z; parser valid/truncated/bad-length+20000ASan/UBSan fuzz cases; R7 uses canonical output |
| Software H264/HEVC first callbacks | Paired FFmpeg0004/VT0005 decode-order candidate native/browser proof; media owner request; separate from canonical HW R7 |
| Startup cache I/O throttle | Private process DefaultIO mapping with real cache; startup/idle observed; no global policy change |
| HTTP(S) open(1) | Private candidate4/4 argv/default/document controls and actual running-Firefox URL delivery; TERM owns canonical merge; owner defaults untouched |
| CPU screenshot skips external buffers | Existing renderer capture used for proof; ordinary CPU capture limitation remains with DESK |
|60fps/presentation pace | Repeated actual measurements below target; parent rendering/export/synchronization prominent; no unsupported causal assignment to compositor/kernel |

The selected Finch-derived media frameworks remain the media owner's builds. Their source notices/licenses are retained by those recipes. This integration does not adopt an entire Finch framework. Private AVFoundation extra classes come from pinned open PureDarwin source with its original MPL-2.0/BSD-2-Clause notices preserved in the output. Mozilla source references remain MPL-2.0; the original bundle and icon are never committed.

## Preserved failures and rejected controls

Old results are not rewritten as passes. These distinctions matter when reviewing the candidate.

| Earlier result | What it establishes / correction |
|---|---|
| baseline001 exit255/XUL absent | Initial installed stack cannot load XUL; private compatibility and sandbox work followed |
| Initial SW-requested windows002–004 | Not qualified full-browser software reference evidence; final R7 original reference receipt failed because its crop included text; the independently corrected actual target regions match exactly, as detailed above |
| CGL installed control139; absent-EGL diagnostic | Installed older EGL selection fails; private guarded error and correct Asahi hardware controls pass |
| Original GPU CGL atomic trap | Write-combined shared header is unsafe for atomics; WRITEBACK correction passes |
| Earlier CGL child blocked in render-node open | Own pending-kill child ultimately reaped by scheduled kernel reboot; not a successful sharing control |
| CPU scene screenshots of GPU windows | Transparent/missing pixels do not prove panel output; actual source=renderer capture supplies proof |
| GPU001/002 XUL load failure | Trial omitted AVFoundation.pd library directory; corrected command used later |
| GPU003/004/menu006/installer r3 | Missing AppKit C alias silently selected installed AppKit; no candidate A/B evidence |
| Provider005/006 automated false | Whitespace PID/start-time parser bug; original false stays, raw-image reanalysis stored separately |
| AVFoundation probev1; asset-only browser/installer | Probe install-name error corrected by relinking; asset-only base omitted AVCaptureDevice and was rejected; current v3 preserves capture absence and assets |
| Native menu0206v1/v2 and browser006 | v1 double-counts menu; v2 loaded system AppKit; corrected native8controls pass but browser causality not isolated; excluded |
| nativeUI003/004 parent SIGSEGV | Retained layer referenced freed context;0207 lifetime fix; later CmdW whole-window behavior is a separate desktop interception |
| contextmenu011 crash | Missing setAppearance selector;0212 native inheritance/lifecycle and actual popup pass; first deallocation probe needed a nested autorelease pool |
| clipboard012 crash/windowless guard | Our ctypes MIME NSString lifetime bug and a windowless event-loop guard are harness failures; corrected retained MIME and observed-empty Wayland controls pass |
| nativeUI017 overall false | Trusted drag/clipboard/scroll/shortcut controls individually pass; Command-W remains false |
| quality001/panels001 | Download inspected before bytes arrived; native Open had no selected file; corrected002 controls pass |
| audio001 timeout | Actual30s session timeout corrected with explicit WebDriver timeout; final current-HAL run passes |
| videoHW001 VP9301/1 | Initial dropped frame stays in raw result; later cold/canonical/GPU runs each300/0 |
| Software H264/HEVC old policies | Withheld first callbacks stall Firefox; native paired decode-order fix and observed complete software005 follow;005 first clip retains a stale deadline error despite actual end |
| fullGPUvideo001 | Stale autoplay AbortError; fixture autoplays only on explicit1 and clears only new measurement's old playError;002/003 pass,001 unchanged |
| publicDASH001 | Socket timeout; corrected002 passes |
| R7 MotionMark001 | Stock score completes but final renderer capture reports NoCurrentScene, consistent with display idle; overall failure retained. Repeat002 passes using guarded pointer-only wake pulses, no workload/idle-setting change |
| First JIT parser / JetStream001 | Trailing omitted null columns caused parser failure; separate raw reanalysis passes. JetStream started before rows existed; owned runner aborted/cleaned, no score;002 completes |
| GPU014 ON and015 OFF | Both lose callbacks while display asleep; no cache-causality inference; awake/admitted same-boot repeats used instead |
| CoreText009 startup stall | Live/stalled failure remains;010 passes after CIO22, with no Foundation/reboot causal attribution |
| AppKit-only CJK index attempt | No effect because active resolver is Onyx; actual Onyx0217 controls pass |
| R4 launcher mode | EACCES observed before first execution; mode corrected and recorded before any launch. R5/R6/R7 preparer emits0755 before cold transfer |

## Reproduce and review integration

Work on Linux in `integrations/puredarwin/firefox/`; use only the open repository toolchain. Read AGENTS, the full FX brief and `runs/m1-coord/RULES.md` first. Never enter another stream's announced reboot, quiet measurement or guarded install. Every screen/seat/GPU sequence holds workstation `gui.lock`, checks `coord-gate.py` inside that lock and logs actual start/end. Builds happen outside the lock. Use strict known-host checking; never restart the shared compositor or signal another stream's process.

On this workstation the verified acquisition, retained Nix outputs, fixture media and cold-staged tools are preserved in ignored paths. To assemble a new review prefix with the same retained providers:

```sh
python3 integrations/puredarwin/firefox/prepare-install.py \
  --release /private/var/db/sffx/release160-review1 \
  --acquisition private/mac-apps/third-party/firefox-160.0 \
  --output runs/firefox/stage-release160-review1 \
  --provider-overrides runs/firefox/release160-r7-provider-overrides.json \
  --gpu-presentation --gpu-dirty-frames --gpu-export-cache --gpu-context-cleanup --font-quality
```

Default prints the exact plan. Add `--build` to assemble the host tree. Retained provider overrides reproduce this workstation's qualified source closures; a different host must retain or rebuild and qualify those inputs. CGL/sandbox/fontconfig overrides are deliberately refused because their paths must match the new prefix. A fresh source build without the retained provider map follows the moving owners' sources and requires new qualification.

After coordination admission, `put.py --source runs/firefox/stage-release160-review1 --destination /private/var/db/sffx/release160-review1 --receipt runs/firefox/put-review1.json` cold-verifies all regular files and symlinks before execution. Never transfer into an existing path or hash an executed Mach-O as pristine. Cold-stage a matching tools directory first; retained `stage-v48` contains the exact tools used for the R6 repeated timing and GPU stability run. Final benchmark repeat/idle tools are retained in `stage-v52`; do not substitute later source when auditing an earlier run.

Use host `launch.py --tools <private-tools> --bundle <release>/Firefox.app --run <fresh-name> -- ...` while holding the GUI lock. Final-release commands use `--executable firefox-sf`, default I/O, sandboxes/cache, `webgl.disable-angle=true`, integer `browser.tabs.inTitlebar=0`, `--gpu-surfaces --gpu-presentation --gpu-vector-compare --gpu-cache-lookups --gpu-dirty-frames --gpu-export-cache`, `--font-config <release>/fonts/fonts.conf` and `--presentation-seat`. `--expect-appkit-prefix <release>/appkit --sample-path /private/var/db/sffx/probes-v3/sf-sample` proves actual providers before timing. Keep diagnostic `--profile-fixture` separate from timing runs. Three same-browser measurements use `--measure-fixture --scroll-fixture --fixture-repetitions 3`.

Qualify the actual outer launcher and URL route with `--allow-remote --integration-launcher <release>/open/bin/open-url --integration-bundle <release>/Firefox.app`; include the owned quality-page, about:support and HTTPS example.com tabs. `--render-quality 0` checks the controlled colour/glyph page. For three-codec video use `--measure-all-videos --avd-counters` with deferred owned video fixtures. For stability use `--browse-stability --seconds 1900 --presentation-seat` in an explicitly announced continuous30min slot; all ten sites, per-visit loads, IPC shutdown notifications and GPU identity must pass.

Derived analyses are reproducible without modifying raw results:

```sh
python3 integrations/puredarwin/firefox/analyze-presentation.py runs/firefox/gpu-repetitions-001
python3 integrations/puredarwin/firefox/analyze-native-presentation.py runs/firefox/gpu-presentation-v10
python3 integrations/puredarwin/firefox/analyze-audio-clock.py runs/firefox/audio-gpu-004
python3 integrations/puredarwin/firefox/analyze-idle.py --tabs 1 runs/firefox/idle-1-r7-001
python3 integrations/puredarwin/firefox/analyze-idle.py --tabs 10 runs/firefox/idle-10-r7-001
```

`prepare-install.py` creates an outer Firefox.app with Mozilla's own icon linked from the untouched original bundle. `daily-launch.py` supplies the private frameworks, real broker, default I/O, sandbox policy, fonts and GPU flags; uses its own profile/logs; rejects inherited disabled sandboxes; preserves an existing user.js. It applies the correct titlebar preference only to a fresh profile. The private open(1) candidate routes real HTTP(S) argv to the running parent rather than creating another process/profile.

`owner-install.py` defaults to a concrete read-only plan. It proposes an absent-only `/Applications/Firefox.app` symlink, desktop entry, appended Firefox pin preserving all existing pins, and merged http/https defaults preserving other handlers. It refuses conflicting existing applications/entries. Applying requires exact-prefix qualification and backs up previous settings. The initial assignment stopped at a read-only plan. The owner subsequently explicitly ordered installation; the system layout uses `install-system.py` and `uninstall.py`, described below. The Cocoa owner separately owns canonical open(1) integration. Do not treat a passing launcher/URL component as a claim that60fps, pinch or acoustic checks passed.

The owner still needs a short physical check: panel sharpness/colours at150%, pointer/menu/typing feel, real trackpad scroll and momentum, and speaker output/lip sync. Pinch and Command-W first require their tracked implementation/classification fixes. DRM/Widevine remains out of scope.

Required repository checks are `tools/check.py --sanitize --cross`, `tools/check_mutations.py`, `tools/lab.py validate`, `tools/lab.py plan`, `tools/acquire_sources.py plan` and `tools/publish_github.py` in its default **plan-only** mode. Final checkpoint v11 passes all six after the measurement and packaging changes. All43 integration Python files compile, and the final scoped diff check passes. The native resource and presentation controls were separately cross-built and observed on the M1 as detailed above. Required unified-patch context retains upstream whitespace; actual added CGL lines are clean. No `--execute` publication or push was performed by FX.

The actual M1 read-only R7 owner plan is retained in `runs/firefox/owner-plan-r7-m1.json`. It proposes `/Applications/Firefox.app`, the Mozilla icon, desktop entry, appended pin and merged http/https handlers while preserving existing settings. Qualification record `/private/var/db/sffx/qualification-r7-v1/qualification.json` explicitly records launcher/URL component success and overall quality-incomplete; installer activation stays false. This earlier R7 qualification remains quality-incomplete. The subsequent system installation uses explicit owner authorization and separate dock/URL evidence; it does not relabel R7 as fully quality-qualified.

Scoped commits: `9aca8a97f` releases owned QuartzCore resources, with the rejected batching experiment excluded by default; `b3ce6b45a` adds bounded CGL export reuse and pixel/resize/adoption controls; `37f6739cc` packages R7 and records the final benchmark/idle/audio/reference methods. Earlier FX commits are52690fec7,fe4abcfe7,7f5fdaa81,bbf326d2c and244879336. No push was performed by FX.

## Active system installation continuation (15 November)

The owner explicitly ordered installation and continued work, rather than a handover. `prepare-install.py --system-layout --release /usr/local/lib/siliconforest/firefox/160-system1` prepares the installed layout. `install-system.py` copies the original 110-file Mozilla bundle unchanged into `/Applications/Firefox.app/Contents/Resources/Mozilla.app`; the outer app's executable supplies its versioned system providers. All 653 regular files and all links were cold-verified before the hidden staging bundle was exposed atomically. No executed Mach-O was copied back as a pristine source. AppKit-family canonical replacements remain TERM-owned.

The pre-install pins file was empty. Its exact bytes are backed up; Terminal, Activity Monitor and Firefox are now pinned together without theme/scale changes. The desktop entry uses the compositor's own `--desktop-request application org.mozilla.firefox`, with the original bundle icon. After CODEX-TB's independently scheduled reboot, its fresh dock catalog displays all three icons; no dock/compositor restart was performed by FX.

Actual `system-dock-normal-002` passes ordinary compositor launch, a mapped external GPU window, actual source=renderer Example Domain pixels and a same-parent URL request. This run has no Marionette/diagnostic arguments and uses the installed default profile. The first check sampled before first paint and is not evidence of a permanent startup failure. A later capture exposed Firefox's welcome overlay over the already loaded Example Domain tab. Supported welcome/preonboarding and telemetry preferences now apply to new profiles and only the byte-exact earlier settings file generated by this installer; an owner-edited user.js is preserved. No terms/consent button was clicked. Sandbox helpers' expected headless messages are not attributed to the parent window.

HTTP(S) LaunchServices defaults are set to Firefox. The system-prefix URL helper opens Example Domain in the existing browser. Canonical `/usr/bin/open` still treats HTTPS as a filesystem path (observed failure); the proven patch is prioritized with TERM. This remaining integration failure is explicit.

Uninstall after quitting Firefox: `python3 /usr/local/lib/siliconforest/firefox/160-system1/uninstall.py --runtime /usr/local/lib/siliconforest/firefox/160-system1 --apply`. Its default is a plan. It checks the installation ownership marker, refuses to signal live Firefox, removes this app/runtime/desktop entry, and preserves profiles/logs, other pins and later owner edits to URL handlers. A host-only filesystem roundtrip proves preservation of earlier and later pins; no M1 uninstall has been performed against the installed owner app.


The continuation after system installation remains active. Canonical CGL was installed with a hardlink rollback and exact native core-profile/cross-process gates, then Terminal, Activity Monitor and Firefox window/renderer gates. The app still selects its qualified per-app Cocoa/GPU cohort until TERM adopts that cohort. The post-reboot ordinary dock check at18:48 mapped Restore Session; the support URL helper returned0 but did not display Example Domain. This failed check is retained separately from the earlier successful normal002 dock/URL capture. Canonical open still used filesystem handling in that check; TERM has merged URL handling for its next gated install. Four current pins, including Firefox beside Activity Monitor and Terminal, were preserved.

The frame pipeline diagnostic now collects all-process Gecko native/JS/IPC profiles, WebRender markers and optional WindowServer composition phase timings. Pipeline003 recorded599 compositions in10s: median2.157ms, p952.815ms, maximum4.716ms; median import/base/draw/finish phases0.027/0.848/0.145/1.066ms. It received396 distinct presentation callback stamps. These stamps are compositor event-loop elapsed time sent after observed page-flip completion, **not hardware timestamps**; exact flip sequence/phase evidence has been requested from DESK. Profiling runs are excluded from timing qualification. Earlier one-URL diagnostic001/002 froze after four initial presents; those failures remain.

Unprofiled30s scroll measured45.06fps at71.70%onecore with the prior GL provider. The expanded real Mesa GL API provider measured51.73fps at74.55%onecore in a later30s run; different warm state prevents attributing that difference to the API exports. Neither reaches60. A paired same-browser main-thread QoS experiment requested default/interactive/default/interactive through real pthread APIs and recorded44.89/53.39/54.19/53.69fps. The warm pair shows no improvement, so installed priority is unchanged. Instantaneous CPU/cluster observations are not sustained affinity measurements.

A review of the original strict pixel evidence found a coordinate error: the eight pixels exceeding one level are glyph edges above the WebGL canvas, included by a displaced screen-space crop. All four complete opaque canvas borders independently establish the actual480×300 region; all144000 WebGL pixels match byte-exactly. The complete60×60 CSS motion rectangle also matches all3600 pixels. `runs/firefox/reference-hw-001/corrected-region-audit.json` records source hashes, bounds and the original failure. Decision: keep hardware rendering, with text-edge AA/rounding tracked separately; no exact arithmetic cause for those glyph differences is proven. Marker-calibrated native recaptures remain a separate regression check.


Private NV12 consumer qualification (19:34:55–19:35:00UTC) passes core3.2/4.1 × video/full-range, each with two revisions and cross-process sharing: all6534 bytes match per case, zero CGL CPU upload/readback bytes, actual GPU plane blits, and old rectangle textures remain exact after source storage is overwritten following consumer completion. Local/global use counts return0; three malformed layouts are rejected. The per-process count matches the required producer predicate; abnormal process death can quarantine a lease, which is a safe leak rather than automatic kernel cleanup. BGRA core-profile/cross-exec regressions pass. The optional `nv12Backing=true` providers remain private; installed providers are unchanged. These tests prove the consumer path, not end-to-end decoded zero-copy. The current kernel lacks AVD import capability, so the pending real-browser path retains one decoder-to-GPU-buffer CPU copy.

The expanded GL dispatch exports use real matching Mesa entry points, including timer queries. A real512×512 FBO triangle returns an exact control pixel, GL0, a59,916ns elapsed query and2,206,000ns CPU wall; invalid-target rejection passes. Earlier empty-work queries returning1ns remain failed controls. This establishes executable query support, not the full browser GPU frame cost.

The installed Firefox uninstall helper was atomically updated19:34:54–55 to remove only this install's Firefox pin, even for an older receipt listing neighbouring apps. Earlier/later/default pins survive all three host roundtrip controls. Cold verification, backup and target plan-only check pass; no app, profile or executed Mach-O changed.


The first actual high-rate private Firefox NV12 run is a retained failure: both1080p60 H264 and4K30 HEVC return media error3, zero output frames/rVFC callbacks, and only6 AVD completions with no kernel errors. Its read-only GPU counter probe passes actual PID/lstart, image UUID and32-byte Mach read admission; zero upload counts in failed playback do not prove successful video transport. The cause is an overly strict new IOSurface layout validator: native CoreVideo NV12 uses a512-byte planar header and a top-level flattened row distinct from the per-plane Y stride. The corrected validator permits bounded nonzero offsets and independent top rows, and requires UV to start after Yoffset+Ysize. The eight-case GPU control with these layouts passes on the M1 at20:02:04–05UTC: every case compares6534 exact bytes, zero CGL CPU transport, completion-bound source reuse and local/global counts0; all four malformed layouts are rejected. The corrected media owner producer separately returns600 actual GPU-backed frames and matching hashes, retaining1866240000 copied bytes. The subsequent actual browser run is recorded below. The previous four-case consumer proof remains valid for its explicit Y0 layouts, rather than a general CoreVideo compatibility result.

Fresh native JIT controls now execute42 through both MAP_JIT and RW→RX memory. The installed API reports pthread JIT thread-write protection unsupported; execution does not establish W^X enforcement. The observed monotonic clock resolution and minimum step are both1000ns. The read-only task/thread probe succeeds across the owned browser family: nice0, main threads QoS tier4/base priority31, with other worker policies recorded separately. This is not evidence of sustained P/E placement. Content Baseline/Ion observation and pristine parent/content allow-jit entitlements remain independently established.

On compositor desk6-005, actual private R7 native UI controls now pass Command-W: Command-T changes3→4 tabs, Command-W returns4→3 and preserves one chrome window and focus. The later fixture portion of this combined run times out because the runner shortens its remaining budget after native UI; it is retained as a failed overall run and supplies no frame/pixel qualification. The separate software marker reference passes: two12×12 opaque anchors establish scale1.5 and actual origin(120,283), independent of the inaccurate mozInnerScreen coordinates. It measures43.665 completion batches/s at65.04%onecore, rAF59.976. In10.004s,614 distinct client frame requests yield437 completion stamps; the compositor reports446 renders and8 late commits, render median1.94/p993.98/max4.77ms. The final frame-clock budget is20ms despite much shorter actual renders. These data motivate a bounded runtime latch on/off/on comparison; no compositor code or installed pacing default has been changed.


A published context point is TRV Consulting's own MacBook Air M1 measurement: Firefox140.0.2 scores28.9 on Speedometer3.1 (30June2025); Firefox141.0 also28.9 (28July2025). [Original measurement ledger](https://trvconsulting.blot.im/are-browsers-on-macos-getting-faster). These are macOS results from older Firefox releases, not a matched Linux/M1 baseline and not our measurements. Our22.6 is about22% lower than28.9; this motivates profiling rather than establishing a particular framework cause. The same-machine Linux request remains with TB for an existing Linux visit; FX performs no reboot.


The corrected actual Firefox NV12 browser run (`browser-nv12-v2`) passes both high-rate clips, with frame599/299 visibly present in separate source=renderer captures.1080p60 H264 decodes600/0drops, AVD+600/noerrors,125.54%onecore over10.706s;4K30 HEVC300/0, AVD+300/noerrors,140.05% over10.546s. UUID-matched GPU process counters report CPUupload/readback0/0 for both, GPU blits2,060,520,448/4,024,901,632bytes including other CGL work. VT separately reports1,866,240,000/3,732,480,000copied bytes, exactly one decoder row-copy per frame. Thus the second CGL CPU upload is removed, while decoded end-to-end zero-copy remains blocked on real kernel import. rVFC remains252/250callbacks over10s, median40.02/p9550ms; zero browser drops are not a60fps presentation result. Verification image readback/capture and CGL transport counters are distinct.

A same-browser runtime pacing A/B at20:03:46–20:05:13 measures15s long scroll phases latch on/off/on:54.45/57.27/56.87 completion batches/s, CPU72.58/60.01/54.35%onecore, rAF60 and exact source hash/GL0 throughout. Compositor render medians1.90/1.85/2.01ms. The warm ON result nearly matches OFF, so turning latching off is not adopted as a fix. Original automatic ON mode is observed and restored in finally; no persistent configuration or compositor code changed. Learned history resets are explicit. The20:14:56–20:15:45UTC diagnostic WebRender run overlapped the owner’s reported physical use20:15–20:25 and is entirely invalidated. Its CPU statistics, profiles and GPU history are discarded as qualification evidence; raw artifacts remain. It must be repeated after the invisible virtual seat is installed. A480-query asynchronous native control is prepared to isolate the driver issue; no GPU bottleneck conclusion follows from that history. Phase correlation remains open.

The installed `system-m4-final-004` control at20:32:14–56UTC uses the compositor application request, not a private launcher. Canonical `/usr/bin/open` and both support routing controls deliver exact unique HTTPS URLs to PID10446. Synthetic Command-T changes4tabs to5; Command-W returns4 with the same window and focus. The runtime manifest is restored byte-exactly before an ordinary compositor relaunch, PID10487, without Marionette. Canonical URL routing keeps that same parent. Its immediate renderer capture shows the original Firefox icon, correct URL/title and window, but the page body is still loading; a later same-process body capture is pending. That capture is not treated as completed body rendering.

The high-rate video metadata distinguishes coalesced callbacks from frame accounting: H264252callbacks end at presentedFrames600, mostly advancing2–3frames per callback; HEVC250callbacks end at300, advancing1–2. These browser counters are not independently observed panel flips. The40–50ms callback spacing alone cannot identify a25fps decoder or prove physical presentation.

At20:38:56–20:39:01UTC, capture-only follow-up verifies the same ordinary PID10487/start time displaying the Example Domain body and exact canonical URL. No pointer/key input was injected in this hold. The independent native controls pass480asynchronous queries (360draw/120empty),90not-ready polls, no overflow, maximum77,250ns; sleep255.018ms uses15.25µs threadCPU and busy250ms uses250.002ms. Twelve NV12 cases including legacy GL match6534bytes each with CPUtransport0 and four malformed rejections. The corrected session wrapper returns8 for a missing command, unregisters the observed own service with status0, and immediately reuses that exact private name successfully. No canonical provider is replaced by these controls.


Urgent downloaded-font failure (owner report,20:40UTC onward): installed Firefox’s chrome text is visible while CNN’s navigation, headlines and Google sign-in labels are laid out but unpainted. All ten requested ordinary sites were captured through the actual renderer and reviewed in `installed-site-legibility-002`: CNN and NYTimes lose page-font text; BBC’s consent text is visible while underlying labels are missing; Wikipedia’s generic text is visible; GitHub was blank/loading and remains unqualified; YouTube labels disappear; Reddit’s body text is visible but its Google card loses labels; Amazon serves a readable challenge rather than its shopping page; Google search snippets remain while result headings disappear; Google Fonts loses most of its labels. Capture001 had failed URL requests from a wrong SSH runtime environment and shows CNN repeatedly; it is retained as a failed sweep, not ten-site evidence.

Installed support ON/OFF controls003 confirm the trigger. With `gfx.downloadable_fonts.enabled=true`, CNN’s CSS fonts report loaded and42 visible laid-out text runs exist, but the actual compositor image lacks their text. With the preference false, headlines, navigation, consent text and Google sign-in labels appear. No owner preference was changed: these use isolated diagnostic profiles. Firefox’s own content screenshot can paint text even when the actual GPU compositor image is blank, so it is explicitly excluded from the new renderer-pixel qualification.

The root is `CTFontManagerCreateFontDescriptorFromData`: our implementation writes `/tmp/CoreText.XXXXXX`, then registers a file. Firefox160’s `NativeFontResourceMac::Create` reconstructs downloaded fonts through that API in its sandboxed rendering process. The native API sequence succeeds outside denied writes, but with filesystem writes denied the direct memory CGFont still paints1148pixels while descriptor creation returnsNULL/EPERM and paints0. Private patch0220 retains the graphics font/provider bytes directly in the descriptor and constructs from that same face before registry matching. No sandbox rule is weakened and no global font is registered. Corrected native normal/denied-write controls both paint1148pixels with errno0. Variable TTF/WOFF, CFF OTF and a two-face collection pass, preserving distinct face indices/names/glyph counts. Direct compressed WOFF2 is unsupported by the current FreeType build; Firefox sanitizes downloaded WOFF2 before the native font-resource stage, and an independently decompressed WOFF2 control passes descriptor creation and1085paintedpixels under denied writes. The same CFData C-API input distinguishes oldNULL/EPERM/0pixels from fixed descriptor/errno0/1085pixels.

The first private browser correction attempt failed its actual-renderer capture because the display was asleep; it remains a failure. TERM8 has an URGENT adoption request and the exact0220 patch/build/native evidence. The new legibility gate pairs actual compositor captures with paint-only transparent-text masks, calibrates DOM text rectangles using opaque markers, measures text-pixel differences and requires image review. Load success or DOM text length alone cannot qualify it. A ten-site rerun with the correction and then sustained browsing remain required.

At21:29:28–21:30:17UTC, corrected private browser003 uses the installed support cohort, its normal I/O policy and only the new CoreText provider. Web fonts and sandboxes remain enabled. Actual compositor captures were reviewed at150%: CNN headlines/navigation/consent/Google sign-in labels are visible, and Google Fonts renders its labels and Roboto specimen. Every eligible unoccluded visible run leaves text ink in calibrated paired captures: CNN8/8, Google Fonts43/43. The parent and GPU both load the expected CoreText UUID, and about:support reports hardware WebRender and Mesa Apple M1. This establishes the font correction for those two views. The original whole run stays FAILED because a subsequent CGL-counter diagnostic rejected the installed provider path; its completed font evidence is recorded separately. The diagnostic now admits only that exact owned system provider in addition to private providers.

TERM8 adopted0220 byte-for-byte into the main source. The installed app still forces its older private CoreText, so canonical adoption alone does not repair it. An owned maintenance installer prepares a new versioned provider under the Firefox runtime, checks retained actual-browser qualification and cold hashes, saves the old launcher/manifest, and selects the override in the app's launcher. It does not replace shared CoreText or alter the owner's preferences, dock pins, theme or scale. Actual installation and ordinary compositor-dock CNN capture remain pending behind the desktop/kernel owners' active reservations. The ten-site stability gate now records actual-renderer text masks and requires ink for every eligible visible run; visual review and comparisons with web fonts disabled remain required.
