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
- File system (kernel): unlink and fsync of a 4 KB file take 15 / 14 ms (p50) on the M1's ext4; Firefox's IndexedDB,
  cache and quota threads spend their idle time there.
