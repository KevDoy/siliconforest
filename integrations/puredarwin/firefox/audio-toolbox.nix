# SPDX-License-Identifier: MIT
# Private Firefox compatibility candidate; media owner owns canonical installs.
let
 base = import ../finch/AudioToolbox/install.nix;
 e = import ../finch/environment.nix;
 codec = e.pkgs.runCommand "sffx-aac-cookie-codec-source" {} ''
   mkdir -p $out
   cp ${../finch/AudioToolbox/codec/SFAudioFFmpeg.c} $out/SFAudioFFmpeg.c
   chmod u+w $out/SFAudioFFmpeg.c
   cp ${./aac-cookie.h} $out/sffx-aac-cookie.h
   cd $out
   ${e.pkgs.patch}/bin/patch -p1 < ${./media-patches/0003-aac-esds-cookie.patch}
 '';
in base.overrideAttrs (old: {
  name = "sffx-audio-toolbox-float-format-arm64";
  patches = (old.patches or []) ++ [
    ./media-patches/0001-float-pcm-without-packed-flag.patch
    ./media-patches/0002-reject-incomplete-format-list.patch
  ];
  buildPhase = builtins.replaceStrings
    [ "${../finch/AudioToolbox/codec/SFAudioFFmpeg.c}" ]
    [ "${codec}/SFAudioFFmpeg.c" ] old.buildPhase;
})
