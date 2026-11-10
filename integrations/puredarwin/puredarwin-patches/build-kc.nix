# M1 kernel collection built from the kexts that build-from-patch.nix produces in-tree.
let
  base = import ../m1-trackpad-input-trace/build-kc.nix;
  k = import ./build-from-patch.nix;
  needle = "  -o kernel";
in base.overrideAttrs (old: {
    buildPhase = builtins.replaceStrings [ needle ] [ ''  -kext "${k}/System/Library/Extensions/PDApcieT8103Bridge.kext" \
      -kext "${k}/System/Library/Extensions/IO80211Family.kext" \
      -kext "${k}/System/Library/Extensions/PDBwfm.kext" \
      -o kernel'' ] old.buildPhase;
  })
