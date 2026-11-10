# The all-drivers collection (../../m1-dcp/build-kc.nix, by default with PDAgx and Wi-Fi, on the
# patched kernel) with one change: the PDM1SPIHID bundle is the multitouch build of this
# directory instead of ../../m1-trackpad-input-trace's. Nothing else in the recipe is touched.
{ agx ? true, wifi ? true, slim ? true }:
let
 base = import ../../m1-dcp/build-kc.nix { inherit agx wifi slim; };
 old = import ../../m1-trackpad-input-trace/build-input-trace.nix;
 next = import ./build-multitouch.nix;
 oldBundle = "${old}/System/Library/Extensions/PDM1SPIHID.kext";
 replacement = "${next}/System/Library/Extensions/PDM1SPIHID.kext";
in base.overrideAttrs (prev:
 # exactly one PDM1SPIHID bundle on the link line, or refuse
 assert builtins.stringLength prev.buildPhase
   - builtins.stringLength (builtins.replaceStrings [ oldBundle ] [ "" ] prev.buildPhase)
   == builtins.stringLength oldBundle;
 { buildPhase = builtins.replaceStrings [ oldBundle ] [ replacement ] prev.buildPhase; })
