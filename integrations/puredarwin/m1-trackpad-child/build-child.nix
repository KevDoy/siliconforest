# Isolated T1 enumeration-only experimental bundle, no checkout mutation.
let
 pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
 pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
 inputs = builtins.fromJSON (builtins.readFile ./sdk-build-inputs-001.json);
 names = [ "CMakeLists.txt" "Info.plist" "PDM1KeyboardHW.cpp" "PDM1KeyboardHW.h" "PDM1SPIHID.cpp" "PDM1SPIHID.h" "m1kbd_board.c" "m1kbd_hw.h" "m1kbd_pio.c" "m1kbd_hid.c" "m1kbd_hid.h" "native_protocol/dl_spi_hid.c" "native_protocol/dl_spi_hid.h" "observation_layout.h" "m1tp_enum.c" "m1tp_enum.h" "PDM1SPIHIDChild.inc" "PDM1SPITrackpad.cpp" "PDM1SPITrackpad.h" "m1tp_child_policy.c" "m1tp_child_policy.h" "child_state.h" "trackpad_reference.h" ];
 lab = pkgs.runCommand "puredarwin-m1-passive-trackpad-child-module-source" {} ''
  mkdir -p "$out/native_protocol"
  cp ${./adapter/CMakeLists.txt} "$out/CMakeLists.txt"
  cp ${../m1-keyboard/Info.plist} "$out/Info.plist"
  cp ${../m1-keyboard/PDM1KeyboardHW.cpp} "$out/PDM1KeyboardHW.cpp"
  cp ${../m1-keyboard/PDM1KeyboardHW.h} "$out/PDM1KeyboardHW.h"
  cp ${./adapter/PDM1SPIHID.cpp} "$out/PDM1SPIHID.cpp"
  cp ${./adapter/PDM1SPIHID.h} "$out/PDM1SPIHID.h"
  cp ${../m1-keyboard/m1kbd_board.c} "$out/m1kbd_board.c"
  cp ${../m1-keyboard/m1kbd_hw.h} "$out/m1kbd_hw.h"
  cp ${../m1-keyboard/m1kbd_pio.c} "$out/m1kbd_pio.c"
  cp ${../m1-keyboard/m1kbd_hid.c} "$out/m1kbd_hid.c"
  cp ${../m1-keyboard/m1kbd_hid.h} "$out/m1kbd_hid.h"
  cp ${../m1-keyboard/native_protocol/dl_spi_hid.c} "$out/native_protocol/dl_spi_hid.c"
  cp ${../m1-keyboard/native_protocol/dl_spi_hid.h} "$out/native_protocol/dl_spi_hid.h"
  cp ${../m1-trackpad-enum/observation_layout.h} "$out/observation_layout.h"
  cp ${../m1-trackpad-enum/core/m1tp_enum.c} "$out/m1tp_enum.c"
  cp ${../m1-trackpad-enum/core/m1tp_enum.h} "$out/m1tp_enum.h"
  cp ${./adapter/PDM1SPIHIDChild.inc} "$out/PDM1SPIHIDChild.inc"
  cp ${./adapter/PDM1SPITrackpad.cpp} "$out/PDM1SPITrackpad.cpp"
  cp ${./adapter/PDM1SPITrackpad.h} "$out/PDM1SPITrackpad.h"
  cp ${./adapter/m1tp_child_policy.c} "$out/m1tp_child_policy.c"
  cp ${./adapter/m1tp_child_policy.h} "$out/m1tp_child_policy.h"
  cp ${./child_state.h} "$out/child_state.h"
  cp ${/home/chrisk/src/darwin-lab/runs/m1-desktop-010/trackpad-child-reference-001/trackpad_reference.h} "$out/trackpad_reference.h"
 '';
 checks = pkgs.lib.concatMapStringsSep "\n" (name:
  let sha = inputs.files.${name}.sha256;
  in assert builtins.match "[0-9a-f]{64}" sha != null;
   "echo '${sha}  src/Kernel/Extensions/PDM1SPIHID/${name}' | sha256sum -c -"
 ) names;
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
 pname = "puredarwin-m1-passive-trackpad-child";
 src = pkgs.runCommand "puredarwin-m1-passive-trackpad-child-source" {} ''
  cp -R ${old.src} "$out"
  chmod -R u+w "$out"
  test ! -e "$out/src/Kernel/Extensions/PDM1SPIHID"
  cp -R ${lab} "$out/src/Kernel/Extensions/PDM1SPIHID"
  printf '\nadd_subdirectory(PDM1SPIHID)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
 '';
 postPatch = (old.postPatch or "") + "\n" + checks;
 buildPhase = ''
  runHook preBuild
  ninja -C build-nix PDM1SPIHID.kext
  runHook postBuild
 '';
 installPhase = ''
  runHook preInstall
  bundle=build-nix/src/Kernel/Extensions/PDM1SPIHID/PDM1SPIHID.kext
  test -f "$bundle/Contents/MacOS/PDM1SPIHID"
  mkdir -p "$out/System/Library/Extensions"
  cp -R "$bundle" "$out/System/Library/Extensions/"
  runHook postInstall
 '';
})
