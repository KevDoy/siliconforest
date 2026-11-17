#!/bin/sh
# SPDX-License-Identifier: MIT
# The SiliconForest rename on an installed system: which files of a root still carry an old
# lookup (a device node, a sysctl, an IOKit class, a Mach service, a launchd label, a tool name
# of before the rename), and whether the staged manifest accounts for each of them. Read-only.
#
#   sf_rename_scan.sh [--root DIR] [--out FILE] [STAGE]
#
#   --root   the system's root (default /); a mounted SiliconForest partition works the same
#   --out    where the full list goes (default: standard output only)
#   STAGE    a staged directory (manifest.tsv). Without it every hit is printed as "old"
#
# One line per file that has an old name in its bytes:
#   row   PATH   a row of the manifest installs over it, removes it or keeps it (also: a file
#                below a directory that a keep row names, and the source of a migrate row)
#   OPEN  PATH   nothing in the manifest knows this file: decide before the switch
# Before the switch OPEN must be empty or explained; after the switch the scan of the whole root
# should print only files that are not SiliconForest's (Apple's two applications, Firefox's own
# bundle, Homebrew bottles) and the kept old files under /private/var/db.
# Text that only credits PureDarwin (NOTICE, licence files) matches too: such files are listed,
# they are not errors. The scan looks at file bytes, not at what a program does with them.
set -u
ROOT=/; OUT=; STAGE=
while [ $# -gt 0 ]; do
  case "$1" in
    --root) ROOT=$2; shift;;
    --out) OUT=$2; shift;;
    -h|--help) sed -n '2,/^set -u/p' "$0" | sed '$d' | cut -c3-; exit 0;;
    -*) echo "sf_rename_scan: unknown option $1" >&2; exit 2;;
    *) STAGE=$1;;
  esac
  shift
done
ROOT=${ROOT%/}
[ -d "$ROOT/" ] || { echo "sf_rename_scan: no root $ROOT/" >&2; exit 2; }
[ -z "$STAGE" ] || [ -f "$STAGE/manifest.tsv" ] || { echo "sf_rename_scan: STAGE needs manifest.tsv" >&2; exit 2; }
TAB=$(printf '\t')
# the old lookups (the same list as the stage's own strings check)
PAT='org\.puredarwin\.|/dev/pd[a-z]|kern\.pd_|machdep\.pd|hw\.pd[a-z]|PD(DRM|Apple|M1|Bwfm|Dcp|Agx|Cpu|Aop|GOP|HID|IOSurface|Gpu|Arm|Surface|NVMe|Nvme|Audio)[A-Za-z0-9]|pd_window_position|pd(m1|avd|agx|nvme|dcp|gpu|disp|cpu|text|apcie|speaker)[a-z0-9_]*=|pd-(netd|dhcp|net-up|backlight|wifipm|console-login|set-hostname|dmesg-save)|pd_(cgl|vk|dri|drm|avd|sound|speaker|backlight|touchbar|trackpad|dcp|agx|nvme|hid|gop|libdyld)_|AudioToolbox\.output-pd|/usr/bin/pd(tone|volume|play|video)|puredarwin(gop|input)_drv'
T=$(mktemp "${TMPDIR:-/tmp}/sf-rename-scan.XXXXXX") || exit 1
: > "$T.rows"; : > "$T.dirs"
if [ -n "$STAGE" ]; then
  grep -v '^#' "$STAGE/manifest.tsv" | grep -v '^$' | while IFS=$TAB read -r act path old new mode class note; do
    printf '%s\n' "$path" >> "$T.rows"
    case "$act" in
      keep) printf '%s/\n' "$path" >> "$T.dirs";;
      migrate) from=${note#from=}; printf '%s\n' "${from%%;sed=*}" >> "$T.rows";;
    esac
  done
fi
# not searched: other file systems, the kept files of this rename, device nodes
find "$ROOT/" -xdev \( -path "$ROOT/private/var/db/sf-rename" -o -path "$ROOT/private/var/db/sf-audio-pretest" -o -path "$ROOT/dev" -o -path "$ROOT/nix" \) -prune -o -type f -size -800000 -print 2>/dev/null |
while IFS= read -r f; do
  grep -a -q -E "$PAT" "$f" 2>/dev/null || continue
  p=${f#"$ROOT"}
  if grep -q -x -F "$p" "$T.rows"; then printf 'row\t%s\n' "$p"; continue; fi
  hit=0
  while IFS= read -r d; do case "$p" in "$d"*) hit=1; break;; esac; done < "$T.dirs"
  if [ $hit = 1 ]; then printf 'row\t%s\n' "$p"; else printf 'OPEN\t%s\n' "$p"; fi
done > "$T"
[ -z "$OUT" ] || cp "$T" "$OUT"
grep "^OPEN" "$T"
echo "scanned ${ROOT:-/}: $(grep -c '^row' "$T") files with old names that the manifest accounts for, $(grep -c '^OPEN' "$T") that it does not"
n=$(grep -c '^OPEN' "$T"); rm -f "$T" "$T.rows" "$T.dirs"
[ "$n" = 0 ]
