# The m1-apcie-host kernel collection plus IO80211Family and PDBwfm.
# NOTE: the base still carries PDApcieT8103Bridge with its diagnostic PDBCM4378Probe class,
# which matches the same PCI function (0x442514e4) as PDBwfm and has to go before a boot.
let
  base = import ../m1-apcie-host/build-kc.nix;
  wifi = import ./build-kexts.nix { };
  needle = "  -o kernel";
in base.overrideAttrs (old:
  # refuse a base recipe whose link line moved, rather than silently building it unchanged
  assert builtins.stringLength old.buildPhase
    != builtins.stringLength (builtins.replaceStrings [ needle ] [ "" ] old.buildPhase);
  {
    buildPhase = builtins.replaceStrings [ needle ] [ ''  -kext "${wifi}/System/Library/Extensions/IO80211Family.kext" \
      -kext "${wifi}/System/Library/Extensions/PDBwfm.kext" \
      -o kernel'' ] old.buildPhase;
  })
