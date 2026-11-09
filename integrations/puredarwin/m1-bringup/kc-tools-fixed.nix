# Same locked kc-tools input and dependencies, with one ARM64 layout fix.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
in
pd.inputs.kc-tools.packages.x86_64-linux.default.overrideAttrs (old: {
  pname = "kc-tools-arm64-text-fixed";
  patches = (old.patches or [ ]) ++ [ ./kc-tools-arm64-text.patch ];
})
