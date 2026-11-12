# SPDX-License-Identifier: GPL-2.0
# PDCpuPower.kext of the default desktop collection (../../m1-cpufreq/build-kext.nix: its
# sources, unchanged, read from that stream's directory) with ./cpupower-patches on top and
# ./PDAppleReboot.cpp, ./pd_pmu_scratch.c compiled in: restart and power-off through the SMC
# and the PMU's boot state bytes, as Linux macsmc-reboot (boot-args pdreboot=, pdpmu=).
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-cpupower.nix --no-out-link
let
  pkgs = (import ../../m1-rust-kext/pins.nix).pkgs;
in
(import ../../m1-cpufreq/build-kext.nix).overrideAttrs (old: {
  pname = "puredarwin-cpupower-reboot";
  src = pkgs.runCommand "puredarwin-cpupower-reboot-tree" { nativeBuildInputs = [ pkgs.patch ]; } ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    d="$out/src/Kernel/Extensions/PDCpuPower"
    test -f "$d/PDCpuPower.cpp"
    for f in ${./PDAppleReboot.cpp} ${./PDAppleReboot.h} ${./pd_pmu_scratch.c} ${./pd_pmu_scratch.h}; do
      cp "$f" "$d/''${f#*-}"
    done
    patch -s -d "$d" -p1 < ${./cpupower-patches/0001-PDCpuPower-restart-and-power-off-through-the-SMC.patch}
  '';
})
