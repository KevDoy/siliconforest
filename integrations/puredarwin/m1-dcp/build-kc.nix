# Kernel collection for PDDcp: the bridge collection (../m1-apcie-host/build-kc.nix, no Wi-Fi)
# plus PDGpuDT (publishes the display nodes), PDDRMDri and PDDcp; `--arg agx true` adds PDAgx.
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
  extra = if agx then ''  -kext "${agxKext}/System/Library/Extensions/PDAgx.kext" \
      '' else "";
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = ''
      set -o pipefail
      {
      '' + builtins.replaceStrings [ needle ] [ ''  -kext "${gpuDt}/System/Library/Extensions/PDGpuDT.kext" \
      -kext "${dcp}/System/Library/Extensions/PDDRMDri.kext" \
      ${extra}-kext "${dcp}/System/Library/Extensions/PDDcp.kext" \
      -o kernel'' ] old.buildPhase + ''
      } 2>&1 | tee kc-build.log
      last=$(grep -E '^ *kext\[[0-9]+\] .*vmaddr=0x[0-9a-f]+ size=0x[0-9a-f]+' kc-build.log | tail -1)
      khdr=$(grep -oE 'fileset: kernel hdr vmaddr=0x[0-9a-f]+' kc-build.log | tail -1 | grep -oE '0x[0-9a-f]+')
      va=$(printf '%s' "$last" | grep -oE 'vmaddr=0x[0-9a-f]+' | cut -d= -f2)
      sz=$(printf '%s' "$last" | grep -oE 'size=0x[0-9a-f]+' | cut -d= -f2)
      test -n "$va" -a -n "$sz" -a -n "$khdr"
      end=$(( (va & 0xffffffffff) + sz )); lim=$(( khdr & 0xffffffffff ))
      echo "kext area ends at +$(printf %x $end), kernel header at +$(printf %x $lim), free $(( lim - end )) bytes"
      if [ $(( lim - end )) -lt 65536 ]; then
        echo "the kext area reaches the kernel's __TEXT: this collection does not boot" >&2
        exit 1
      fi
    '';
  })
