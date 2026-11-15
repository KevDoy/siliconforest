# SPDX-License-Identifier: MIT
# Private process font configuration, including one pinned OFL fallback font.
{ prefix ? "/private/var/db/sffx/fonts-v3" }:
let
 a=import ../cocoa/arm64.nix;
 fonts=import ./noto-fonts.nix {pkgs=a.pkgs;};
in a.pkgs.runCommand "sffx-private-font-configuration" {} ''
 mkdir -p $out/cache
 cp ${fonts.font} $out/NotoSansDevanagari-Regular.ttf
 cp ${fonts.license} $out/OFL.txt
 cat > $out/fonts.conf <<'EOF'
 <?xml version="1.0"?>
 <!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
 <fontconfig>
  <dir>/usr/share/fonts</dir>
  <dir>/System/Library/Fonts</dir>
  <dir>${prefix}</dir>
  <cachedir>${prefix}/cache</cachedir>
 </fontconfig>
 EOF
''
