# SPDX-License-Identifier: MIT
# Combined native driver build; pdcio_cm=1 is a separate experimental opt-in.
import ./build-kext-base.nix { connectionManager=true; }
