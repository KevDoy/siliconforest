# Firefox performance on SiliconForest (FXPERF)

Root causes of wasted CPU, GPU and memory in Firefox 160 on our stack, each with the measurement that shows it.
Fixes are handed to the owner of the layer they touch; patches here are files for adoption, not installed by FXPERF.

## Method
- One own Firefox instance per hold (`-no-remote`, profile `/private/var/db/sffxperf/p1`, fresh each run), only
  synthetic input through the compositor's virtual seat, Marionette (port 28291) for navigation, the Gecko profiler
  (all processes, all threads, 2 ms, native stacks) and about:memory dumps.
- `tools/fxperf_m1.py stat`: per process and per thread CPU from `proc_pid_rusage`/`proc_pidinfo` over 30 s, context
  switches, wake-ups, footprint. `regions`: a vmmap-like map from `PROC_PIDREGIONPATHINFO` (resident, private, dirty
  per region and per file). `sffxperf-fontprobe.c`: an `__interpose` dylib for the own instance that logs whole-file
  reads and maps of font files with their caller chain (`FXFONT` lines).
- `tools/mk_perf_launcher.sh`: a private copy of the installed launcher for the own instance only; it can select the
  private GPU provider set (`FXPERF_MANIFEST`) and put candidate providers from `/private/var/db/sffxperf` in front
  (`SFFXPERF_FRAMEWORKS`), so a fix is measured in the real browser before its owner adopts it.
- Analysis offline: `tools/gprof.py` (per-thread busy samples, symbolicated: XUL through Mozilla's symbol server, our
  libraries with llvm-symbolizer), `markers.py` (marker rates), `leafpaths.py` (caller chains of a leaf),
  `memrep.py`, `fontmem.py`. Raw captures stay in `runs/fxperf/<tag>/` (not in git).

## Findings
### Memory: ten private copies of the CJK font in the parent (fixed by `patches/onyx2d-font-files-mapped-once.patch`)
Firefox's AsyncFontInfoLoader (`CTFontInfo::LoadFontFamilyData`) makes a CTFont for every face record. CoreText's
`createGraphicsFont` falls back to `O2DataProviderCreateWithURL` for collection faces and variable instances, and
`-[O2DataProvider initWithURL:]` reads the whole file into a private NSData each time. NotoSansMonoCJK-VF.otf.ttc
(31.9 MB) is read 10 times and the copies stay (CoreText keeps the graphics font per record): 304 MB private dirty
in the parent, plus 3 copies of NotoEmoji. macOS keeps font data mapped from the file (clean, shared pages).
The patch maps an installed font file once per process, read-only, and shares it among providers.
Measured on the M1 (runs/fxperf A2 = before, A3 = with the patch; same scenario: fresh profile, example.com, cnn.com):
| | before | after |
|---|---|---|
| whole-file reads of a font (FXFONT) | 10 (CJK) | 0 |
| private whole-file copies in the parent (region map) | 10 x 30.4 MB + 3 x 1.9 MB | 0 |
| parent heap-allocated (about:memory) | 610.8 MB | 301.3 MB |
| parent resident-phys-footprint | 921.1 MB | 633.8 MB |
| family footprint, example.com idle / cnn.com idle | 1411 / 1904 MB | 1087 / 1623 MB |
The 30 FreeType maps of the same file (one per FT_New_Face) are clean shared pages: they inflate RSS sums
(913 MB of the parent's 1.1 GB RSS) but not physical memory.

### FX-029 (idle cnn.com): the page animates; our per-frame cost is the waste
- Gecko side (profile A2): the content refresh driver ticks every vsync, "HasImageAnimations /
  HasPendingRenderingSteps (Update animations and send events)": CSS animations (46 iterations/s), animated images
  and a muted autoplay video. 60 display lists/s, WebRender builds and composites 60 frames/s. A Mac does the same
  Gecko work; there each frame recomposites only the changed layer.
- Ours, per frame (installed c19 GPU path, clean run D-base-1: no tracing, no profiler, no Marionette, 30 s): family
  101 % of a core; the compositor receives 75 commits/s (55 shown/s), each the full 1920x1440 window with full damage.
  The parent main thread is 38 % CPU and busy 85 % of wall time: 80 % of it in `-[CALayerContext _layerTreeChanged]`
  -> `layerContextNeedsDisplay:`: `-[CARenderer render]` of the whole tree 29 %, the export
  (`copySFFXRenderedSurface` -> `CGLSFFXCopyFramebuffer` -> glFinish) 37 %, WindowServer `sfGPUCompose` 9.5 %;
  43.5 % of its wall time is blocked in glFinish (`drmSyncobjWait`).
- Requests (no patch proven yet): EGLImage per IOSurface in CGL; a GPU-side fence instead of the export glFinish;
  damage-limited render/compose/commit (QuartzCore, WindowServer, compositor); no flush for an unchanged frame.
- Tried and NOT effective on the c19 path (one run each, runtime changed between runs, inside run-to-run noise):
  IOSurface lookup of a known port without mapping (`build/0234`, the parent compositor thread is only 3 % on c19;
  its remaining per-frame maps are probably new video surfaces - inference), one glFinish per CGL write-back (GPU Renderer 8.3 -> 8.4 %).
- CODEX-FX's opt-in 0218 (layer-tree notifications batched to 60 Hz ticks), built on c19 QuartzCore and enabled in my
  instance (runs F-base-1 / F-coal-1, frozen runtime): compositor commits 70 -> 61/s, shown 50 -> 44/s, but family CPU
  101.9 -> 102.5 % and parent main thread 38.7 -> 39.6 %: no CPU gain (one pair; not pursued).
- File system (kernel): unlink and fsync of a 4 KB file take 15 / 14 ms (p50) on the M1's ext4; Firefox's IndexedDB,
  cache and quota threads spend their idle time there.

### FX-031 (idle static page) on c19
- Warm profile, example.com, 30 s after a 45 s settle, no tracing (runs E2, E4): family 2.4 % / 2.1 % of a core,
  248 / 210 context switches/s, 7 interrupt wake-ups/s; no GPU commits in steady 10 s windows (compositor WMPACING).
  Left: the parent main thread (~0.85 %) and Glean's dispatcher thread (~0.7 %).
- The first minutes of a fresh profile cost 19-27 % (E1, E3): IndexedDB/quota first-run work whose threads sit in
  ext4 unlink/fsync (15 / 14 ms per 4 KB file on the M1).

### Memory at 1 / 10 / 30 tabs (c19, after the font fix was installed)
Family sum of RSS / of phys_footprint: 1 tab (example.com, warm) 2.0 / 0.95 GB; 10 static tabs 2.6 / 1.28 GB; 30 tabs
(10 static + 20 Wikipedia) 4.1 / 2.25 GB (FXQA's earlier RSS sums: 2.3 / 4.0 / 5.4 GB). RSS sums overstate: every
process maps the same font files (the parent alone maps NotoSansMonoCJK 31 times = ~980 MB of "RSS", clean shared
pages). Per process, our libraries' private dirty pages are small (Mesa ~5 MB, AppKit/Foundation/libSystem ~1 MB each);
XUL's relocated data is 55-66 MB per process (Mozilla's, as on macOS). Gecko's own accounting at 30 tabs: explicit
602 MB, heap-allocated 772 MB across all processes.

### FX-030 (tab switch)
Installed c19, 12 Command-number switches among 10 static tabs (run B2, no profiler, inject request time -> the page's
Date.now, FXQA's method): visibilitychange p50 80 ms, second rAF p50 117 ms, p95 297 ms (unchanged from FXQA's 119 ms).
Partial attribution (profile B1 with 1 ms sampling, inflated):
Key -> AsyncTabSwitch:Start 3-5 ms; the content repaints at once (PaintWhileInterruptingJS) and the GPU process
composites the new tab within ~3 ms (DidComposite); AsyncTabSwitch:Finish at ~21 ms; but the parent's compositor
thread receives the native-layer commit for that frame only ~55 ms after Start and renders it (CARenderer) at ~57 ms,
then the window present + compositor flip (~22 ms commit->flip) follow. The ~50 ms between the GPU composite and the
parent's commit is not explained yet (not a busy thread: the GPU Renderer and the parent compositor thread wait).

### Start-up (installed c19 path, warm profile, 3 runs)
exec -> first presented frame 2.00 / 1.75 / 1.82 s (median 1.82 s); window listed by the compositor ~2.6 s (polled).
