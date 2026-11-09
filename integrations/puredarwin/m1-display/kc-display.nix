{ pkgs, kcTools, kernel, kernelSource, genericKexts, platformKexts, displayKexts, kcRecipe, extraKexts ? [ ] }:
let
  kexts = pkgs.runCommand "puredarwin-m1-display-kext-selection" { } ''
    set -eu
    dest="$out/System/Library/Extensions"
    mkdir -p "$dest"
    for bundle in ${genericKexts}/System/Library/Extensions/*.kext; do
      name=$(basename "$bundle")
      case "$name" in
        PDArmPlatformExpert.kext|IOGOPFramebuffer.kext) ;;
        *) ln -s "$bundle" "$dest/$name" ;;
      esac
    done
    platform=${platformKexts}/System/Library/Extensions/PDArmPlatformExpert.kext
    display=${displayKexts}/System/Library/Extensions/IOGOPFramebuffer.kext
    test -f "$platform/Contents/MacOS/PDArmPlatformExpert"
    test -f "$display/Contents/MacOS/IOGOPFramebuffer"
    ln -s "$platform" "$dest/PDArmPlatformExpert.kext"
    ln -s "$display" "$dest/IOGOPFramebuffer.kext"
  '';
in
pkgs.callPackage kcRecipe {
  inherit kcTools kernel kernelSource kexts extraKexts;
  kcBase = "fffffe0006000000";
}
