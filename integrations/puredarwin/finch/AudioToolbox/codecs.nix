# SPDX-License-Identifier: MIT
# Full Finch file/PCM sources plus LGPL FFmpeg AAC/MP3/ALAC decoding.
let
  e = import ../environment.nix;
  ffmpeg = import ./ffmpeg.nix;
  notices = import ./ffmpeg-licenses.nix;
  base = import ./build.nix;
in base.overrideAttrs (old: {
  name = "siliconforest-finch-AudioToolbox-codecs-arm64";
  patches = (old.patches or []) ++ [ ./codec/0001-compressed-audio.patch ];
  buildPhase = builtins.replaceStrings
    [ "mkdir objects" "${e.cc} -dynamiclib objects/*.o" "-lCoreFoundation -lSystem" ]
    [ ''
        cp ${./codec/SFAudioFFmpeg.h} SFAudioFFmpeg.h
        CF="$CF -I$PWD -I${ffmpeg}/usr/include"
        mkdir objects
      '' "${e.cc} $CF -c ${./codec/SFAudioFFmpeg.c} -o objects/SFAudioFFmpeg.o\n${e.cc} $CF -c ${./codec/PDQueue.c} -o objects/PDQueue.o\n${e.cc} -dynamiclib objects/*.o" "-lCoreFoundation -Wl,-reexport_library,${./codec/retained-provider.tbd} -L${ffmpeg}/usr/lib -lavformat -lavcodec -lswresample -lavutil -Wl,-unexported_symbols_list,${./codec/hidden-symbols.txt} -lSystem" ] old.buildPhase;
  installPhase = old.installPhase + ''
    cp ${./codec/retained-provider.tbd} $out/System/Library/Frameworks/AudioToolbox.framework/Versions/A/AudioToolbox.output-pd.tbd
    mkdir -p $out/System/Library/Frameworks/AudioToolbox.framework/Versions/A/Resources/Licenses/FFmpeg
    cp ${notices}/* $out/System/Library/Frameworks/AudioToolbox.framework/Versions/A/Resources/Licenses/FFmpeg/
  '';
})
