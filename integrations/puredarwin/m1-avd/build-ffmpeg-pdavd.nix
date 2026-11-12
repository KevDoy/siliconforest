# SPDX-License-Identifier: MIT
# FFmpeg 8.1.2 for the SiliconForest arm64 userland (./player/build-ffmpeg.nix) with the
# `pdavd` hwaccel (./ffmpeg/0001-pdavd-hwaccel.patch: hardware decode of H.264, HEVC and VP9
# through /dev/pdavd) and libpdavd (./build-user.nix) linked in. Use: `ffmpeg -hwaccel pdavd`,
# or in a program av_hwdevice_ctx_create(..., AV_HWDEVICE_TYPE_PDAVD / "pdavd", ...).
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes \
#     --no-out-link --cores 4 --max-jobs 1 build-ffmpeg-pdavd.nix
let
  user = import ./build-user.nix;
in import ./player/build-ffmpeg.nix {
  extraPatches = [ ./ffmpeg/0001-pdavd-hwaccel.patch ];
  extraConfigureFlags = [ "--enable-pdavd" "--enable-hwaccel=h264_pdavd" "--enable-hwaccel=hevc_pdavd"
                          "--enable-hwaccel=vp9_pdavd" ];
  extraBuildInputs = [ user ];
  extraCFlags = [ "-I${user}/include/pdavd/compat" ];
  extraLibs = [ "-lpdavd" ];
  nameSuffix = "-pdavd";
}
