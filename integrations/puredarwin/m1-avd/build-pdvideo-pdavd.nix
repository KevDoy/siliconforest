# SPDX-License-Identifier: MIT
# pdvideo (./player/build-pdvideo.nix) linked against the FFmpeg build with the pdavd hwaccel
# (./build-ffmpeg-pdavd.nix): the player with hardware decoding. `gl = true` builds pdvideo-gl.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes \
#     --no-out-link --cores 4 --max-jobs 1 build-pdvideo-pdavd.nix [--arg gl true]
{ gl ? false }:
let
  user = import ./build-user.nix;
in import ./player/build-pdvideo.nix {
  ffmpeg = import ./build-ffmpeg-pdavd.nix;
  extraLdFlags = [ "-L${user}/lib" ];
  extraLibs = [ "-lpdavd" ];
  nameSuffix = "-pdavd";
  inherit gl;
}
