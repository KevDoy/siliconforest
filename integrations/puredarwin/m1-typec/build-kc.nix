# SPDX-License-Identifier: MIT
# Experimental collection only. The daily collection stays unchanged until booted.
import ../rebase/build-kc.nix {
  platform = import ../m1-native-boot/build-platform.nix;
  cpupower = import ../m1-native-boot/reboot/build-cpupower.nix;
  omit = [ "PDSg2002Eth" "PDSg2002SD" "PDSun60iDisplay" "PDQcomGeniI2C"
           "PDQcomTLMM" "PDPowerVR" "ext4" ];
  more = [
    { name = "PDAppleNVMeRW"; from = import ../m1-storage/build-kext-rw.nix; }
    { name = "ext4"; from = import ../m1-storage/build-ext4.nix; }
    { name = "PDAppleTypeC"; from = import ./build-kext.nix; }
  ];
}
