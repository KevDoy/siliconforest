# SPDX-License-Identifier: MIT
# Experimental GPU IOSurface bridge, private prefix only.
{prefix ? "/private/var/db/sffx/gl-gpu-v3"}:
let base=import ./cgl.nix {inherit prefix;};
in base.overrideAttrs(old:{patches=old.patches++[./cgl-patches/0002-gpu-iosurface-alias-and-rectangle-blit.patch ./cgl-patches/0003-export-framebuffer-and-import-layer-gpu-storage.patch];})
