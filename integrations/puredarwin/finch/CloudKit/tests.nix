# SPDX-License-Identifier: MIT
import ../tests/conformance.nix {
  name = "CloudKit";
  arc = true;
  programs = [ "cloudkit-unavailable-test" "cloudkit-test" ];
}
