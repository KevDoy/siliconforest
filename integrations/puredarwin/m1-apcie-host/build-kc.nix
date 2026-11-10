# native50 KC plus the GPL-2.0 PDApcieT8103Bridge host bridge kext.
let
  base = import ../m1-trackpad-input-trace/build-kc.nix;
  bridge = /nix/store/q0nws595rrwibpbw7pg6alf238db99ir-puredarwin-apcie-host-0.1/System/Library/Extensions/PDApcieT8103Bridge.kext;
  needle = "  -o kernel";
in base.overrideAttrs (old: {
    buildPhase = builtins.replaceStrings [ needle ] [ ''  -kext "${bridge}" \
      -o kernel'' ] old.buildPhase;
  })
