# Kernel collection for milestone boot 2: PDGpuDT (powers GFX, publishes gpu@206400000),
# PDDRMDri (the /dev/dri devices every DRM kext depends on) and PDAgx, on
#   wifi = true  (default)  ../m1-bwfm/build-kc.nix: bridge + IO80211Family + PDBwfm
#   wifi = false            ../m1-apcie-host/build-kc.nix: the bridge collection only
#     nix-build build-agx-kc.nix --arg wifi false
# PDRustHello, PDRustLKPI and PDRustPlat are not in either (16 MiB payload limit between the
# collection header and the kernel's __TEXT; PDRustPlat would also bind the same node).
# Boot with `pdgpudt=2 pdgpudt_stringprops=1 pdagx=1`; without pdagx=1 PDAgx does not attach.
{ wifi ? true }:
let
  base = if wifi then import ../m1-bwfm/build-kc.nix else import ../m1-apcie-host/build-kc.nix;
  agx = import ./build-agx-kext.nix;
  gpuDt = import ../m1-gpu-dt/build-kext.nix;
  needle = "  -o kernel";
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = builtins.replaceStrings [ needle ] [ ''  -kext "${gpuDt}/System/Library/Extensions/PDGpuDT.kext" \
      -kext "${agx}/System/Library/Extensions/PDDRMDri.kext" \
      -kext "${agx}/System/Library/Extensions/PDAgx.kext" \
      -o kernel'' ] old.buildPhase;
  })
