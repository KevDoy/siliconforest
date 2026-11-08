let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  p = pd.packages.x86_64-linux;
  xkb = p.xkbcommon-arm64.override {
    withX11 = false;
    libxcb = null;
    libXau = null;
    libXdmcp = null;
  };
in pkgs.symlinkJoin {
  name = "puredarwin-chonkstep-target-deps";
  paths = [ p.iokit-arm64 xkb ];
}
