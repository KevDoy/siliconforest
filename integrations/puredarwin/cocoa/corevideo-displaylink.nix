# SPDX-License-Identifier: MIT
# CoreVideo as CODEX installs it (../finch/CoreVideo/pd-displaylink.nix: Finch's buffers and formats, PureDarwin's
# CVDisplayLink.m) with TERM9's 0149: the display link ticks on the compositor's clock (libWindowServer 0148w).
let
  a = import ./arm64.nix;
  base = import ../finch/CoreVideo/pd-displaylink.nix;
  link = a.pkgs.runCommand "corevideo-displaylink-src" { } ''
    mkdir -p $out/src/Frameworks/CoreVideo
    cp ${a.src}/src/Frameworks/CoreVideo/CVDisplayLink.m $out/src/Frameworks/CoreVideo/
    chmod -R u+w $out
    cd $out && patch -p1 < ${./patches/0149-corevideo-display-link-on-the-compositor-clock.patch}
  '';
in base.overrideAttrs (old: {
  name = "siliconforest-corevideo-displaylink-clock-arm64";
  buildPhase = builtins.replaceStrings [ "-c ${a.src}/src/Frameworks/CoreVideo/CVDisplayLink.m" ]
    [ "-c ${link}/src/Frameworks/CoreVideo/CVDisplayLink.m" ] old.buildPhase;
})
