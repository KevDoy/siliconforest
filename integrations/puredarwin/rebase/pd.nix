# SPDX-License-Identifier: MIT
# The PureDarwin source every lab recipe builds from, and the one switch between the pinned
# revision and the rebase target. Recipes say
#   pd = (import ../rebase/pd.nix).pd;
# instead of naming a revision themselves.
#
#   SF_PD unset, empty or "tip"   the default since 12 November 2026: upstream's tip below
#   SF_PD=pin                     the revision the lab was pinned to until then (e7aa00a6), with
#                                 the same flake reference the recipes carried, so every pin
#                                 derivation is the one it always was
#
# The switch is an environment variable because the recipes import each other by path and
# nix-build has no argument that reaches every import. Two caches in this lab key on recipe
# text, not on the environment: m1-boot-speed/tools/bs_nix_path.py and qemu-dev/qd_image.py
# (runs/qemu-dev/base/inputs.json). Whoever sets SF_PD also sets BS_NIX_CACHE=0 (that cache is
# then neither read nor written) and does not run qd_image.py with it: a tip path left in
# either cache would be handed to the next pin run. docs/roadmap/issues/SF-V-rebase.md.
#
# `tip` was upstream-xnu-12377 on 11 November 2026 (clone: sources/puredarwin-rebase, which
# is this stream's own; sources/puredarwin-upstream-next belongs to the Cocoa stream).
let
  lab = "/home/chrisk/src/darwin-lab";
  sources = {
    pin = { checkout = "sources/puredarwin-baseline"; rev = "e7aa00a60f0b56ef35aa24252e81467787bf71ea"; };
    tip = { checkout = "sources/puredarwin-rebase"; rev = "c713d8fb9b048a38e2242d67e29b9957ced8c495"; };
  };
  asked = builtins.getEnv "SF_PD";
  which = if asked == "" then "tip" else asked;
  source = sources.${which} or (throw "SF_PD=${asked}: expected pin or tip");
in rec {
  inherit which;
  inherit (source) rev;
  tip = which == "tip";
  # one of two values by source, for the few places where a recipe must differ
  pick = { pin, tip }: if which == "tip" then tip else pin;
  pd = builtins.getFlake "git+file://${lab}/${source.checkout}?submodules=1&shallow=1&rev=${source.rev}";
  pkgs = import pd.inputs.nixpkgs { system = "x86_64-linux"; };
}
