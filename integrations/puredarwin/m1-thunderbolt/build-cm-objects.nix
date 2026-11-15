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
    chmod -R u+w "$dest/pininc"
    cp -R ${./nativeinc}/. "$dest/pininc/"
    cp ${./native}/*.h ${./native}/*.c "$dest/"
    cp ${./include/pd_tb_registry.h} "$dest/pd_tb_registry.h"
    cp ${./src/pd_tb_registry.c} "$dest/pd_tb_registry.c"
    python3 ${./native/patch_device.py} "$out"
    cp ${./pd_tb_cm_compat.h} "$dest/pd_tb_cm_compat.h"
    cp ${./include/pd_tb_wire.h} "$dest/pd_tb_wire.h"
    chmod -R u+w "$dest"
    # Tracepoints are instrumentation only; the native port has no Linux
    # tracepoint subscribers. Keep every protocol/control operation intact.
    python3 - "$dest/driver" <<'PY'
import pathlib,re,sys
p=pathlib.Path(sys.argv[1]);names=set()
q=p/'domain.c';s=q.read_text();needle='int tb_domain_init(void)\n{';assert s.count(needle)==1
q.write_text(s.replace(needle,needle+'\n\tif (!pd_tb_native_ready()) return -EPROBE_DEFER;'))
# Intel x86 Apple NHI resume device-links do not apply to the ARM platform.
q=p/'tb.c';s=q.read_text();a=s.index('static bool tb_apple_add_links(struct tb_nhi *nhi)\n{');b=s.index('\nstruct tb *tb_probe',a)
s=s[:a]+'#ifdef CONFIG_X86\n'+s[a:b]+'\n#else\nstatic bool tb_apple_add_links(struct tb_nhi *nhi) { return false; }\n#endif\n'+s[b:];q.write_text(s)
for source in p.glob('*.c'):
 names.update(re.findall(r'\b(trace_tb_\w+)\s*\(',source.read_text()))
with (p/'trace.h').open('w') as f:
 f.write('/* SPDX-License-Identifier: GPL-2.0 */\n/* Linux tracepoint instrumentation disabled in the native port. */\n')
 for name in sorted(names):f.write('#define '+name+'(...) '+('false' if name.endswith('_enabled') else '((void)0)')+'\n')
PY
    cp ${./cm-CMakeLists.txt} "$dest/CMakeLists.txt"
    printf '\nadd_subdirectory(PDThunderboltCM)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
  '';
  # The inherited hand-written configurePhase does not consume cmakeFlags.
  configurePhase = let
    phase = builtins.replaceStrings
      [ "cmake -S . -B build-nix -G Ninja" ]
      [ "cmake -S . -B build-nix -G Ninja -DPD_TB_CM_FULL=${if full then "ON" else "OFF"}" ]
      old.configurePhase;
  in assert phase != old.configurePhase; phase;
  buildPhase = ''
    runHook preBuild
    ninja -C build-nix -k 0 PDThunderboltCM
    runHook postBuild
  '';
  installPhase = ''
    mkdir -p "$out/objects"
    find build-nix/src/Kernel/Extensions/PDThunderboltCM/CMakeFiles/PDThunderboltCM.dir -name '*.o' -exec cp {} "$out/objects/" \;
    expected=${if full then "21" else "12"}
    test "$(find "$out/objects" -name '*.o' | wc -l)" -eq "$expected"
    ${if full then "test -f \"$out/objects/domain.c.o\"" else "test ! -f \"$out/objects/domain.c.o\""}
    mkdir -p "$out/nix-support"
    nm="$NIX_DARWIN_TOOLCHAIN_DIR/$NIX_DARWIN_HOST-nm"
    "$nm" -g --defined-only -j "$out"/objects/*.o | sort -u > "$out/nix-support/defined"
    "$nm" -u -j "$out"/objects/*.o | sort -u > "$out/nix-support/undefined"
    comm -23 "$out/nix-support/undefined" "$out/nix-support/defined" > "$out/nix-support/required-external"
    cp ${./NOTICE} "$out/NOTICE"
  '';
})
