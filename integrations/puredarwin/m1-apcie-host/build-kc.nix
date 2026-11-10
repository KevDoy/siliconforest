# native50 KC plus the GPL-2.0 PDApcieT8103Bridge host bridge kext.
let
  base = import ../m1-trackpad-input-trace/build-kc.nix;
  bridge = /nix/store/vcbccv78a82y4i15kd6x7ll2ndkyh962-puredarwin-apcie-host-0.1/System/Library/Extensions/PDApcieT8103Bridge.kext;
  needle = "  -o kernel";
in base.overrideAttrs (old: {
    buildPhase = builtins.replaceStrings [ needle ] [ ''  -kext "${bridge}" \
      -o kernel'' ] old.buildPhase;
  })
