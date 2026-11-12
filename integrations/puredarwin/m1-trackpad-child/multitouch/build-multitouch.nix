# PDM1SPIHID with the trackpad in multitouch mode. The same module layout as
# ../../m1-trackpad-input-trace/build-input-trace.nix, which builds the bundle in the current
# collections: the keyboard, transport, enumeration, child-policy and trace sources are taken
# unchanged from their directories; PDM1SPIHID.{cpp,h}, PDM1SPIHIDChild.inc, PDM1SPITrackpad.{cpp,h}
# and CMakeLists.txt come from this directory (modified copies), plus the new decoder.
let
 pd = (import ../../rebase/pd.nix).pd;
 pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
 lab = pkgs.runCommand "puredarwin-m1-spi-hid-multitouch-module-source" {} ''
  mkdir -p "$out/native_protocol"
  cp ${./CMakeLists.txt} "$out/CMakeLists.txt"
  cp ${../../m1-keyboard/Info.plist} "$out/Info.plist"
  cp ${../../m1-keyboard/PDM1KeyboardHW.cpp} "$out/PDM1KeyboardHW.cpp"
  cp ${../../m1-keyboard/PDM1KeyboardHW.h} "$out/PDM1KeyboardHW.h"
  cp ${./PDM1SPIHID.cpp} "$out/PDM1SPIHID.cpp"
  cp ${./PDM1SPIHID.h} "$out/PDM1SPIHID.h"
  cp ${../../m1-keyboard/m1kbd_board.c} "$out/m1kbd_board.c"
  cp ${../../m1-keyboard/m1kbd_hw.h} "$out/m1kbd_hw.h"
  cp ${../../m1-keyboard/m1kbd_pio.c} "$out/m1kbd_pio.c"
  cp ${../../m1-keyboard/m1kbd_hid.c} "$out/m1kbd_hid.c"
  cp ${../../m1-keyboard/m1kbd_hid.h} "$out/m1kbd_hid.h"
  cp ${../../m1-keyboard/native_protocol/dl_spi_hid.c} "$out/native_protocol/dl_spi_hid.c"
  cp ${../../m1-keyboard/native_protocol/dl_spi_hid.h} "$out/native_protocol/dl_spi_hid.h"
  cp ${../../m1-trackpad-enum/observation_layout.h} "$out/observation_layout.h"
  cp ${../../m1-trackpad-enum/core/m1tp_enum.c} "$out/m1tp_enum.c"
  cp ${../../m1-trackpad-enum/core/m1tp_enum.h} "$out/m1tp_enum.h"
  cp ${./PDM1SPIHIDChild.inc} "$out/PDM1SPIHIDChild.inc"
  cp ${./PDM1SPIHIDMultitouch.inc} "$out/PDM1SPIHIDMultitouch.inc"
  cp ${./PDM1SPITrackpad.cpp} "$out/PDM1SPITrackpad.cpp"
  cp ${./PDM1SPITrackpad.h} "$out/PDM1SPITrackpad.h"
  cp ${../adapter/m1tp_child_policy.c} "$out/m1tp_child_policy.c"
  cp ${../adapter/m1tp_child_policy.h} "$out/m1tp_child_policy.h"
  cp ${../child_state.h} "$out/child_state.h"
  cp ${../../m1-trackpad-input-trace/core/m1tp_trace.c} "$out/m1tp_trace.c"
  cp ${../../m1-trackpad-input-trace/core/m1tp_trace.h} "$out/m1tp_trace.h"
  cp ${./m1tp_mt.c} "$out/m1tp_mt.c"
  cp ${./m1tp_mt.h} "$out/m1tp_mt.h"
  cp ${./m1tp_mt_selftest.c} "$out/m1tp_mt_selftest.c"
  cp ${./NOTICE} "$out/NOTICE"
  cp ${/home/chrisk/src/darwin-lab/runs/m1-desktop-010/trackpad-child-reference-001/trackpad_reference.h} "$out/trackpad_reference.h"
 '';
in pd.packages.x86_64-linux.kexts-arm64.overrideAttrs (old: {
 pname = "puredarwin-m1-spi-hid-multitouch";
 src = pkgs.runCommand "puredarwin-m1-spi-hid-multitouch-source" {} ''
  cp -R ${old.src} "$out"
  chmod -R u+w "$out"
  test ! -e "$out/src/Kernel/Extensions/PDM1SPIHID"
  cp -R ${lab} "$out/src/Kernel/Extensions/PDM1SPIHID"
  printf '\nadd_subdirectory(PDM1SPIHID)\n' >> "$out/src/Kernel/Extensions/CMakeLists.txt"
 '';
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
  cp ${./NOTICE} "$out/System/Library/Extensions/PDM1SPIHID.kext/Contents/NOTICE"
  runHook postInstall
 '';
})
