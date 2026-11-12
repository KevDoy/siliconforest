# SPDX-License-Identifier: MIT
# PDM1SPIHID.kext (the built-in keyboard and trackpad, ../m1-trackpad-child/multitouch/
# build-multitouch.nix) with one addition: it tells PDAppleTouchBar about the Fn key and
# about typing (./spihid). The trackpad stream's sources are taken as they are; the patch
# and the three new files are applied to a copy of its build tree.
let
  base = import ../m1-trackpad-child/multitouch/build-multitouch.nix;
  pd = (import ../rebase/pd.nix).pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
in base.overrideAttrs (old: {
  pname = "puredarwin-m1-spi-hid-touchbar";
  src = pkgs.runCommand "puredarwin-m1-spi-hid-touchbar-source" { nativeBuildInputs = [ pkgs.patch ]; } ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    d="$out/src/Kernel/Extensions/PDM1SPIHID"
    test -f "$d/PDM1SPIHID.cpp"
    cp ${./spihid/PDM1SPIHIDTouchBar.inc} "$d/PDM1SPIHIDTouchBar.inc"
    cp ${./spihid/pd_hid_fn.c} "$d/pd_hid_fn.c"
    cp ${./spihid/pd_hid_fn.h} "$d/pd_hid_fn.h"
    cp ${./spihid/pd_touchbar_kbd.h} "$d/pd_touchbar_kbd.h"
    patch -d "$d" -p1 < ${./spihid/spihid-touchbar.patch}
  '';
})
