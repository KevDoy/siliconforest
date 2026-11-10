#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only OR MIT
# Regenerates ../patches from an edited copy of the pin's drivers/gpu/drm/apple.
#   regen.sh setup <workdir>   drv.orig/ = the pin, drv/ = the pin + current patches; edit drv/
#   regen.sh split <workdir>   0001 = apple_drv.c, 0002 = every other changed file
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); top=$(cd "$here/.." && pwd); P=$top/patches
pin=$(cd "$top/../../.." && pwd)/sources/gpu-plan/linux-aurora/drivers/gpu/drm/apple
cmd=${1:?setup|split}; work=$(mkdir -p "${2:?workdir}" && cd "$2" && pwd)
norm() { sed -E 's|^--- drv.orig/([^\t]*).*|--- a/\1|; s|^\+\+\+ drv/([^\t]*).*|+++ b/\1|' | grep -v '^diff -u'; }
case $cmd in
setup)
	rm -rf "$work/drv.orig" "$work/drv"; cp -R "$pin" "$work/drv.orig"; chmod -R u+w "$work/drv.orig"
	cp -R "$work/drv.orig" "$work/drv"
	for p in "$P"/*.patch; do patch -d "$work/drv" -p1 --no-backup-if-mismatch < "$p" > /dev/null; done;;
split)
	cd "$work"
	{ cat "$here/0001.msg"; diff -u drv.orig/apple_drv.c drv/apple_drv.c | norm || true; } > "$P/0001-local-variable-not-named-jiffies.patch"
	{ cat "$here/0002.msg"; for f in $(diff -rq drv.orig drv | awk '{print $2}' | sed 's|^drv.orig/||' | grep -v '^apple_drv.c$' | sort); do diff -u "drv.orig/$f" "drv/$f" | norm || true; done; } > "$P/0002-pddcp-stage-lines.patch"
	for p in "$P"/*.patch; do echo "$(basename "$p"): +$(grep -c '^+[^+]' "$p") -$(grep -c '^-[^-]' "$p")"; done;;
esac
