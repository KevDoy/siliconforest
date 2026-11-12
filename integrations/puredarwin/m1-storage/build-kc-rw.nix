# SPDX-License-Identifier: MIT
# The default collection at the tip plus PDAppleNVMeRW.kext (./build-kext-rw.nix: the
# WRITE-FENCED build of the internal NVMe driver) INSTEAD of the read-only PDAppleNVMe.kext.
# Only the fenced-write boots of the disk-root work name this file; tools/default_kc.sh never
# does. Without pdnvme=3, the three partition arguments and pdnvme_write=1 nothing is written.
#   nix-build build-kc-rw.nix --no-out-link        (add --arg native true for the native-boot collection)
# The write build's code is 7 KB larger than the read-only one's and crosses a 16 KiB segment
# boundary, which the collection has no room for (about 8 KB before the kernel image). Every
# boot kext is on the launcher's required-member list, so room is made by leaving out one of
# the ADDED members in THIS collection only: PDAppleAVD (the video decoder, which does nothing
# without pdavd=1). No default collection is changed. The build ends with the overlap check.
{ kernelConfig ? "development", native ? false }:
assert (import ../rebase/pd.nix).tip;
let
  base = import ../rebase/build-kc.nix ({
    inherit kernelConfig;
    more = [ { name = "PDAppleNVMeRW"; from = import ./build-kext-rw.nix; } ];
  } // (if native then { platform = import ../m1-native-boot/build-platform.nix; } else { }));
  without = phase:
    let parts = builtins.split "-kext \"[^\"]*/PDAppleAVD[.]kext\" [\\\\]\n" phase;
    in assert builtins.length parts == 3;
    builtins.concatStringsSep "" (builtins.filter builtins.isString parts);
# builtins.split drops the string's store references: the empty substring carries them back
in base.overrideAttrs (old: { buildPhase = without old.buildPhase + builtins.substring 0 0 old.buildPhase; })
