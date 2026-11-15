# SPDX-License-Identifier: MIT
# GPU IOSurface bridge; private prefix by default, explicit canonical packaging.
{prefix ? "/private/var/db/sffx/gl-gpu-v3", canonical ? false}:
let base=import ./cgl.nix {inherit prefix canonical;};
in base.overrideAttrs(old:{patches=old.patches++[./cgl-patches/0002-gpu-iosurface-alias-and-rectangle-blit.patch ./cgl-patches/0003-export-framebuffer-and-import-layer-gpu-storage.patch ./cgl-patches/0004-opt-in-export-framebuffer-resource-cache.patch];})
