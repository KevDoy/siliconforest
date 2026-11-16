# TERM12: the canonical CGL (cgl-gpu.nix, as installed) with or without FXPERF2's 0303 (rect export, EGLImage per surface)
{ with0303 ? true, extra ? [ ] }:
let
  F = ../../..;   # integrations/puredarwin/firefox
  base = import (F + "/cgl-gpu.nix") { prefix = "/usr/local/lib/siliconforest/opengl/v1"; canonical = true; nv12Backing = true; };
in base.overrideAttrs (old: { patches = old.patches ++ (if with0303 then [ (F + "/perf/patches/0303-cgl-rectangle-export-and-image-per-surface.patch") ] else [ ]) ++ extra; })
