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
- Ours, per frame: CGL writes every IOSurface WebRender drew back with a glFinish per surface
  (`pd_cgl_write_back` -> `drmSyncobjWait`); the parent's `IOSurfaceLookupFromMachPort` maps and unmaps each tile's
  dma-buf at every native-layer commit (94 % of the parent compositor thread's busy samples); the parent main thread
  renders the whole layer tree and the WindowServer composes and damages the whole window.
