# SPDX-License-Identifier: MIT
# Compile the actual pinned Linux software CM/control-channel against LinuxKPI.
# Experimental object target: not a kext, native NHI service or hardware test.
{ full ? true }:
let
  lkpi = import ../m1-rust-kext/lkpi;
  inherit (import ../m1-rust-kext/pins.nix) pkgs;
in lkpi.drmKextFinal.overrideAttrs (old: {
  pname = "puredarwin-thunderbolt-cm-objects";
  src = pkgs.runCommand "puredarwin-thunderbolt-cm-tree" { nativeBuildInputs = [ pkgs.python3 ]; } ''
    cp -R ${lkpi.finalTree} "$out"
    chmod -R u+w "$out"
    dest="$out/src/Kernel/Extensions/PDThunderboltCM"
    mkdir -p "$dest"
    cp -R ${./upstream/family} "$dest/driver"
    cp -R ${./pininc} "$dest/pininc"
    cp ${./pd_tb_cm_compat.h} "$dest/pd_tb_cm_compat.h"
    cp ${./include/pd_tb_wire.h} "$dest/pd_tb_wire.h"
    chmod -R u+w "$dest"
    # Tracepoints are instrumentation only; the native port has no Linux
    # tracepoint subscribers. Keep every protocol/control operation intact.
    python3 - "$dest/driver" <<'PY'
import pathlib,re,sys
p=pathlib.Path(sys.argv[1]);names=set()
for source in p.glob('*.c'):
 names.update(re.findall(r'\b(trace_tb_\w+)\s*\(',source.read_text()))
with (p/'trace.h').open('w') as f:
 f.write('/* SPDX-License-Identifier: GPL-2.0 */\n/* Linux tracepoint instrumentation disabled in the native port. */\n')
 for name in sorted(names):f.write('#define '+name+'(...) '+('false' if name.endswith('_enabled') else '((void)0)')+'\n')
PY
    cp ${./cm-CMakeLists.txt} "$dest/CMakeLists.txt"
    printf '\nadd_subdirectory(PDThunderboltCM)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  cmakeFlags = (old.cmakeFlags or []) ++ [ "-DPD_TB_CM_FULL=${if full then "ON" else "OFF"}" ];
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix -k 0 PDThunderboltCM
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    cp build-nix/src/Kernel/Extensions/PDThunderboltCM/CMakeFiles/PDThunderboltCM.dir/driver/*.o "$out/objects/"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
