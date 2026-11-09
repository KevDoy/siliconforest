# Canonical libSystem recipe (build-libsystem-pagesize.nix: baseline e7aa00a6 plus the
# vm_page_size patch) with one more patch: posix_spawnattr_init zeroes the whole
# attribute struct before setting its defaults. Source-only until reviewed.
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
in pd.packages.x86_64-linux.libsystem-arm64.overrideAttrs (old: {
  pname = "puredarwin-libsystem-spawnattr";
  patches = (old.patches or []) ++ [ ./fix-root/vm-page-size-symbol.patch ./fix-spawnattr/posix-spawnattr-init-zero.patch ];
})
