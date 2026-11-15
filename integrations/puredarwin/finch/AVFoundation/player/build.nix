# SPDX-License-Identifier: MIT
# Staged player-state/JPEG surface; NOT the installed H264/HAL movie backend.
let
 e=import ../../environment.nix;
 a=import ../../../cocoa/arm64.nix;
 base=import ../../media/bottle/avfoundation.nix;
in base.overrideAttrs(old:{
 name="siliconforest-finch-AVFoundation-player-stage-arm64";
 preBuild=(old.preBuild or "")+''
   mkdir -p playerheaders/AVFoundation
   cp ${../include}/AVFoundation/AVFoundation.h playerheaders/AVFoundation/SFAssetCore.h
 '';
 buildPhase=builtins.replaceStrings
  [ "-I${../include}" "AVAssetWriterInputGroup; do" "-lCoreGraphics -lFoundation" "-I${e.pd.outPath}/src/Frameworks" "mkdir -p objects headers/AudioToolbox" ]
  [ "-I${./include} -I$PWD/playerheaders -I${../include}" "AVAssetWriterInputGroup AVPlayer AVPlayerItemMediaDataCollector; do" "-L${a.appDeps.quartzcore}/usr/lib -lQuartzCore -lCoreGraphics -lFoundation" "-I${a.appDeps.quartzcore}/usr/include -I${a.appDeps.appkit}/usr/include -I${e.pd.outPath}/src/Frameworks" "mkdir -p objects headers/AudioToolbox playerheaders/AVFoundation; cp ${../include}/AVFoundation/AVFoundation.h playerheaders/AVFoundation/SFAssetCore.h; substituteInPlace playerheaders/AVFoundation/SFAssetCore.h --replace-fail ',*AVCustomMediaSelectionScheme;' '; @class AVCustomMediaSelectionScheme;'" ] old.buildPhase;
})
