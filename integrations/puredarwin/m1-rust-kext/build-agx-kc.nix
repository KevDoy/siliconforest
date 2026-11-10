# Kernel collection for milestone boot 2: PDGpuDT (powers GFX, publishes gpu@206400000),
# PDDRMDri (the /dev/dri devices every DRM kext depends on) and PDAgx, on
#   wifi = false (default)  ../m1-apcie-host/build-kc.nix: the bridge collection only
#   wifi = true             ../m1-bwfm/build-kc.nix: bridge + IO80211Family + PDBwfm
#     nix-build build-agx-kc.nix --arg wifi true
#     DOES NOT FIT today: the kexts end exactly at the kernel's __TEXT and the recipe refuses
#     (see the check at the end of buildPhase).
# PDRustHello, PDRustLKPI and PDRustPlat are not in either (16 MiB payload limit between the
# collection header and the kernel's __TEXT; PDRustPlat would also bind the same node).
# Boot with `pdgpudt=2 pdgpudt_stringprops=1 pdagx=1`; without pdagx=1 PDAgx does not attach.
{ wifi ? false }:
let
  base = if wifi then import ../m1-bwfm/build-kc.nix else import ../m1-apcie-host/build-kc.nix;
  agx = import ./build-agx-kext.nix;
  gpuDt = import ../m1-gpu-dt/build-kext.nix;
  needle = "  -o kernel";
  # the kernel with ../m1-dcp/kernel-patches (pd_lfile descriptors: SCM_RIGHTS, select/poll/
  # kevent wakeups) in place of the pinned one; the layer needs its pd_lfile_wakeup
  onKernel = import ../m1-dcp/kernel-swap.nix;
  pkgs = (import ./pins.nix).pkgs;
  marginPages = 4;
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    nativeBuildInputs = (old.nativeBuildInputs or [ ]) ++ [ pkgs.python3 ];
    buildPhase = ''
      set -o pipefail
      {
      '' + builtins.replaceStrings [ needle ] [ ''  -kext "${gpuDt}/System/Library/Extensions/PDGpuDT.kext" \
      -kext "${agx}/System/Library/Extensions/PDDRMDri.kext" \
      -kext "${agx}/System/Library/Extensions/PDAgx.kext" \
      -o kernel'' ] (onKernel old.buildPhase) + ''
      } 2>&1 | tee kc-build.log
      # kc-builder accepts a payload that ends exactly at the kernel's __TEXT, and the
      # launcher's image check then rejects the collection (boot ag02: "Overlapping
      # file-backed segments map different bytes"). Require the last kext to end at least
      # ${toString marginPages} pages before the kernel header.
      # The launcher's image gate, reproduced (tools/kc_overlap.py): kc-builder puts the
      # collection's __PRELINK_INFO after the last kext and accepts it running into the
      # kernel's __TEXT; such a collection is refused at boot time.
      python3 ${./tools/kc_overlap.py} kernel
    '';
  })
