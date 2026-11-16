# mdnsd (the mDNSResponder daemon, mDNSPosix) from the default PureDarwin source with
# mdnsd-debug-quiet.patch: -debug no longer turns on every LogInfo() (FXNET; see the patch).
# Same upstream CMake target and libSystem as ../../m1-network-native-resolver/build-mdnsd.nix.
let
  pd = (import ../../rebase/pd.nix).pd;
  system = import ../../m1-userspace-diagnostics/build-libsystem-spawnattr.nix;
  selected = pd.packages.x86_64-linux.userland-arm64.override {
    pname = "puredarwin-mdnsd-quiet";
    buildTargets = [ "mdnsd" ];
    prebuiltLibSystem = system;
    xorgDriverIncludes = null;
    installUserlandTargetsOnly = true;
  };
in selected.overrideAttrs (old: {
  patches = (old.patches or [ ]) ++ [ ./mdnsd-debug-quiet.patch ];
  installPhase = ''
    mkdir -p "$out/usr/sbin"
    cp build-nix/src/Userspace/mdnsd/mdnsd "$out/usr/sbin/"
    grep -q 'debug \[-verbose\]' src/Libraries/mDNSResponder/mDNSPosix/PosixDaemon.c
  '';
  dontFixup = true;
  dontStrip = true;
})
