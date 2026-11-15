# SPDX-License-Identifier: MIT
# Private/staged only: preserve exact retained display-link family and add an
# opaque destructor-owned lifetime slot. No mutable-attachment ownership trick.
let e=import ../../environment.nix; base=import ../../CoreVideo/pd-displaylink.nix; in
base.overrideAttrs(old:{
 name="siliconforest-corevideo-frame-lifetime-stage-arm64";
 postPatch=(old.postPatch or "")+''
  python3 ${./corevideo-port.py} userland/CoreVideo/CVPixelBuffer.c
 '';
})
