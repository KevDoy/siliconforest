# SPDX-License-Identifier: MIT
# build-pdvideo.nix with the GL output (usr/bin/pdvideo-gl and the Mesa bundle it loads).
import ./build-pdvideo.nix { gl = true; }
