# SPDX-License-Identifier: MIT
# Builds PDCpuPower.kext (GPL-2.0: T8103 cluster performance states that follow load, under
# a die-temperature limit read from the SMC) in the pinned PureDarwin tree. Sources: ./kernel,
# ./src and ../m1-thermal.
#   nix-build --extra-experimental-features nix-command --extra-experimental-features flakes build-kext.nix
let
  pd = (import ../rebase/pd.nix).pd;
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  copied = pkgs.runCommand "puredarwin-cpupower-source" {} ''
    mkdir -p "$out"
    cp ${./kernel/CMakeLists.txt} "$out/CMakeLists.txt"
    cp ${./kernel/Info.plist} "$out/Info.plist"
    cp ${./kernel/PDCpuPower.cpp} "$out/PDCpuPower.cpp"
    cp ${./src/pd_cpufreq_policy.c} "$out/pd_cpufreq_policy.c"
    cp ${./src/pd_cpufreq_policy.h} "$out/pd_cpufreq_policy.h"
    cp ${../m1-thermal/src/pd_thermal_policy.c} "$out/pd_thermal_policy.c"
    cp ${../m1-thermal/src/pd_thermal_policy.h} "$out/pd_thermal_policy.h"
    cp ${../m1-thermal/kernel/PDSmcSession.cpp} "$out/PDSmcSession.cpp"
    cp ${../m1-thermal/kernel/PDSmcSession.h} "$out/PDSmcSession.h"
  '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
  pname = "puredarwin-cpupower";
  src = pkgs.runCommand "puredarwin-cpupower-tree" {} ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    test ! -e "$out/src/Kernel/Extensions/PDCpuPower"
    cp -R ${copied} "$out/src/Kernel/Extensions/PDCpuPower"
    printf '\nadd_subdirectory(PDCpuPower)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix PDCpuPower.kext
    runHook postBuild
  '';
  installPhase = ''
    runHook preInstall
    bundle=build-nix/src/Kernel/Extensions/PDCpuPower/PDCpuPower.kext
    test -f "$bundle/Contents/MacOS/PDCpuPower"
    mkdir -p "$out/System/Library/Extensions"
    cp -R "$bundle" "$out/System/Library/Extensions/"
    runHook postInstall
  '';
})
