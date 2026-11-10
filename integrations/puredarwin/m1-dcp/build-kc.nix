# Kernel collection for PDDcp: the bridge collection (../m1-apcie-host/build-kc.nix, no Wi-Fi)
# plus PDGpuDT (publishes the display nodes), PDDRMDri and PDDcp; `--arg agx true` adds PDAgx.
# The kernel is ./kernel.nix (the pinned bring-up kernel + ./kernel-patches), not the pinned
# binary: see kernel-swap.nix.
# Boot-args: the ones m1-gpu-dt documents for publishing the display nodes, and pddcp=1|2.
# Fails when fewer than 4 pages are left before the kernel's __TEXT (see
# ../m1-rust-kext/build-agx-kc.nix for why).
{ agx ? false }:
let
  base = import ../m1-apcie-host/build-kc.nix;
  dcp = import ./build-kext.nix;
  agxKext = import ../m1-rust-kext/build-agx-kext.nix;
  gpuDt = import ../m1-gpu-dt/build-kext.nix;
  needle = "  -o kernel";
  # the kernel with ../m1-dcp/kernel-patches (pd_lfile descriptors: SCM_RIGHTS, select/poll/
  # kevent wakeups) in place of the pinned one; the layer needs its pd_lfile_wakeup
  onKernel = import ./kernel-swap.nix;
  pkgs = (import ../m1-rust-kext/pins.nix).pkgs;
  extra = if agx then ''  -kext "${agxKext}/System/Library/Extensions/PDAgx.kext" \
      '' else "";
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    nativeBuildInputs = (old.nativeBuildInputs or [ ]) ++ [ pkgs.python3 ];
    buildPhase = ''
      set -o pipefail
      {
      '' + builtins.replaceStrings [ needle ] [ ''  -kext "${gpuDt}/System/Library/Extensions/PDGpuDT.kext" \
      -kext "${dcp}/System/Library/Extensions/PDDRMDri.kext" \
      ${extra}-kext "${dcp}/System/Library/Extensions/PDDcp.kext" \
      -o kernel'' ] (onKernel old.buildPhase) + ''
      } 2>&1 | tee kc-build.log
      # The launcher's image gate, reproduced (tools/kc_overlap.py): kc-builder puts the
      # collection's __PRELINK_INFO after the last kext and accepts it running into the
      # kernel's __TEXT; such a collection is refused at boot time.
      python3 ${../m1-rust-kext/tools/kc_overlap.py} kernel
    '';
  })
