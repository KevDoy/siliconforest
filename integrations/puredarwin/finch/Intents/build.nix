# SPDX-License-Identifier: MIT
let e = import ../environment.nix;
in import ../objc-framework.nix {
  name = "Intents";
  arc = false;
  patches = [ ./patches/0001-own-declarations-input.patch ];
  prepare = ''
    ${e.pkgs.python3}/bin/python3 userland/Intents/generate.py --headers ${./include/Intents} --output userland/Intents/Generated.m
  '';
}
