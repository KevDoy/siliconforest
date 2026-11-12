# SPDX-License-Identifier: MIT
# The default desktop collection (../m1-boot-speed/build-kc.nix) with the platform kext of
# ./build-platform.nix in place of ../m1-smp/build-platform.nix; everything else is the
# same build. Boots under the hypervisor exactly as the default; natively, with
# pdcpuheld=1, it can start the secondary cores.
#   nix-build build-kc.nix --no-out-link
# Boot-args and launcher that go with it: pd_quiet_wait=0 pd_rtc_wait=0 as the default, and
# ./launcher-native.patch with ./native_boot.py (tools/nb_boot.sh).
{ kernelConfig ? "development" }:
let
  base = import ../m1-boot-speed/build-kc.nix { inherit kernelConfig; };
  from = import ../m1-smp/build-platform.nix;
  to = import ./build-platform.nix;
in base.overrideAttrs (old:
  let phase = builtins.replaceStrings [ "${from}" ] [ "${to}" ] old.buildPhase;
  in assert phase != old.buildPhase; { buildPhase = phase; })
