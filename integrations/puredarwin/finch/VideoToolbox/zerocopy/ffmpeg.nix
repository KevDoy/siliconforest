# SPDX-License-Identifier: MIT
# Keep the upstream pdavd FFmpeg source/recipe, append opt-in buffer import only.
let e=import ../../environment.nix; base=import ../../../m1-avd/build-ffmpeg-pdavd.nix; in
base.overrideAttrs(old:{
 pname="siliconforest-ffmpeg-pdavd-external-capture";
 nativeBuildInputs=(old.nativeBuildInputs or [])++[e.pkgs.python3];
 postPatch=(old.postPatch or "")+''
  cp ${./capture-pool.h} libavutil/sf_pdavd_capture_pool.h
  python3 ${./ffmpeg-port.py} .
 '';
})
