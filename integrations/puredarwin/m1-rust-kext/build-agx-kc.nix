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
  marginPages = 4;
in base.overrideAttrs (old:
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = ''
      set -o pipefail
      {
      '' + builtins.replaceStrings [ needle ] [ ''  -kext "${gpuDt}/System/Library/Extensions/PDGpuDT.kext" \
      -kext "${agx}/System/Library/Extensions/PDDRMDri.kext" \
      -kext "${agx}/System/Library/Extensions/PDAgx.kext" \
      -o kernel'' ] old.buildPhase + ''
      } 2>&1 | tee kc-build.log
      # kc-builder accepts a payload that ends exactly at the kernel's __TEXT, and the
      # launcher's image check then rejects the collection (boot ag02: "Overlapping
      # file-backed segments map different bytes"). Require the last kext to end at least
      # ${toString marginPages} pages before the kernel header.
      last=$(grep -E '^ *kext\[[0-9]+\] .*vmaddr=0x[0-9a-f]+ size=0x[0-9a-f]+' kc-build.log | tail -1)
      khdr=$(grep -oE 'fileset: kernel hdr vmaddr=0x[0-9a-f]+' kc-build.log | tail -1 | grep -oE '0x[0-9a-f]+')
      va=$(printf '%s' "$last" | grep -oE 'vmaddr=0x[0-9a-f]+' | cut -d= -f2)
      sz=$(printf '%s' "$last" | grep -oE 'size=0x[0-9a-f]+' | cut -d= -f2)
      test -n "$va" -a -n "$sz" -a -n "$khdr"
      # 64-bit kernel addresses: compare the low 40 bits
      end=$(( (va & 0xffffffffff) + sz )); lim=$(( khdr & 0xffffffffff ))
      echo "kext area ends at +$(printf %x $end), kernel header at +$(printf %x $lim), free $(( lim - end )) bytes"
      if [ $(( lim - end )) -lt $(( ${toString marginPages} * 16384 )) ]; then
        echo "the kext area reaches the kernel's __TEXT (less than ${toString marginPages} pages free): this collection does not boot" >&2
        exit 1
      fi
    '';
  })
