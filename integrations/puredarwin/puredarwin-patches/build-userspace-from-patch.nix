# Applies the userspace part of 0002 to the pinned tree and builds the pd-dhcp target in-tree.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
  system = import ../m1-userspace-diagnostics/build-libsystem-spawnattr.nix;
  selected = pd.packages.x86_64-linux.userland-arm64.override {
    pname = "puredarwin-pd-dhcp-from-patch";
    buildTargets = [ "pd-dhcp" ];
    prebuiltLibSystem = system;
    xorgDriverIncludes = null;
    installUserlandTargetsOnly = true;
  };
in selected.overrideAttrs (old: {
  src = pkgs.runCommand "puredarwin-pd-dhcp-tree" { nativeBuildInputs = [ pkgs.patch ]; } ''
    cp -R ${old.src} "$out"
    chmod -R u+w "$out"
    cd "$out"
    # the nix registry file is not part of this filtered source; only the Userspace hunks apply
    patch -p1 -f < ${./0002-m1-pcie-wifi-userspace-and-registry.patch} || true
    test -f src/Userspace/pd-dhcp/pd-dhcp.c
    grep -q 'add_subdirectory(pd-dhcp)' src/Userspace/CMakeLists.txt
  '';
  installPhase = ''
    mkdir -p "$out/usr/bin"
    cp build-nix/src/Userspace/pd-dhcp/pd-dhcp "$out/usr/bin/"
  '';
  dontFixup = true;
  dontStrip = true;
})
