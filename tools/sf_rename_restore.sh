#!/bin/sh
# SPDX-License-Identifier: MIT
# Undoes tools/sf_rename_switch.sh on an installed system, complete or interrupted: every file
# the switch put in place is taken out again (kept under new/), every file it replaced or
# removed is put back from old/. It needs no stage directory: the journal says what was done.
#
#   sf_rename_restore.sh [--dry-run] [--root DIR] [--name NAME]
#
# Reads ROOT/private/var/db/NAME/journal backwards (NAME: default sf-rename, as given to the switch). Afterwards that directory holds
#   new/<path>          the renamed files that were in place (state copies made by the switch
#                       are there too, with whatever the renamed system wrote into them)
#   journal.restored.N  the journal that was undone
# and the root is as it was before the switch, byte for byte, for every path of the manifest.
# The kernel is not this script's: `sf-bootbin-install.sh <PARTUUID> --rollback` from Linux.
set -u
NAME=sf-rename; DRY=0; ROOT=/
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1;;
    --root) ROOT=$2; shift;;
    --name) NAME=$2; shift;;
    -h|--help) sed -n '2,/^set -u/p' "$0" | sed '$d' | cut -c3-; exit 0;;
    *) echo "sf_rename_restore: unknown argument $1" >&2; exit 2;;
  esac
  shift
done
ROOT=${ROOT%/}
case "$NAME" in *[!A-Za-z0-9._-]*|"") echo "sf_rename_restore: --name: letters, digits, . _ - only" >&2; exit 2;; esac
DB=$ROOT/private/var/db/$NAME; OLD=$DB/old; NEW=$DB/new; J=$DB/journal
TAB=$(printf '\t')
say() { printf '%s\n' "$*"; }
die() { printf 'sf_rename_restore: %s\n' "$*" >&2; exit 1; }
[ -f "$J" ] || die "no journal at $J: nothing to restore"
# the journal's lines, last first (no tac on Darwin)
REV=$(mktemp "${TMPDIR:-/tmp}/sf-rename-rev.XXXXXX") || die "mktemp"
awk '{ l[NR] = $0 } END { for (i = NR; i > 0; i--) print l[i] }' "$J" > "$REV"
present() { [ -e "$1" ] || [ -L "$1" ]; }
n=0; bad=0
# ---- checks -----------------------------------------------------------------------------------
while IFS=$TAB read -r what path arg; do
  case "$what" in
    installed)
      [ "$arg" = replaced ] && ! present "$OLD$path" && present "$ROOT$path" && {
        # journalled, but the old file was not kept yet: the switch stopped right after the journal line,
        # and the new file only goes in once the old one is kept, so the path still holds the old file
        say "back $path (was journalled, not yet replaced: left as it is)"; continue; }
      if [ "$arg" = replaced ] && ! present "$OLD$path"; then say "BAD  the old $path is not in old/ and not at its path"; bad=$((bad + 1)); continue; fi
      say "back $path ($arg)";;
    removed)
      if present "$OLD$path"; then say "back $path (removed)"
      elif present "$ROOT$path"; then say "back $path (was journalled, not yet moved: left as it is)"
      else say "BAD  the old $path is not in old/"; bad=$((bad + 1)); fi;;
    migrated) say "take $path (the copy of $arg) to new/";;
    *) say "BAD  journal line: $what $path"; bad=$((bad + 1));;
  esac
  n=$((n + 1))
done < "$REV"
[ "$bad" = 0 ] || { rm -f "$REV"; die "$bad journal entries cannot be undone; nothing was changed"; }
if [ "$DRY" = 1 ]; then rm -f "$REV"; say "dry run: $n journal entries would be undone; nothing was changed"; exit 0; fi
# ---- the restore ------------------------------------------------------------------------------
aside() { # the file now at PATH goes to new/PATH
  present "$ROOT$1" || return 0
  mkdir -p "$NEW$(dirname "$1")" && { rm -rf "$NEW$1"; mv "$ROOT$1" "$NEW$1"; }
}
keep_new() { # the file now at PATH gets a second name, new/PATH; the path itself stays as it is
  mkdir -p "$NEW$(dirname "$1")" || return 1
  rm -rf "$NEW$1"
  if [ -L "$ROOT$1" ]; then cp -P "$ROOT$1" "$NEW$1"; else ln "$ROOT$1" "$NEW$1" 2>/dev/null || cp -p "$ROOT$1" "$NEW$1"; fi
}
while IFS=$TAB read -r what path arg; do
  case "$what" in
    installed)
      rm -f "$ROOT$path.sf-new"
      if [ "$arg" = new ]; then aside "$path" || die "cannot take $path out"
      elif present "$OLD$path"; then
        # the path is never left empty: the file in place is kept under new/, then the old one is
        # renamed over it (when both names are the same file - the switch stopped before the new
        # file went in - the second name in old/ is simply dropped)
        if [ "$ROOT$path" -ef "$OLD$path" ]; then rm -f "$OLD$path"
        else
          if present "$ROOT$path"; then keep_new "$path" || die "cannot keep the new $path"; fi
          mv -f "$OLD$path" "$ROOT$path" || die "cannot put the old $path back (it is in old/)"
        fi
      fi;;
    removed)
      if present "$OLD$path"; then mv "$OLD$path" "$ROOT$path" || die "cannot put $path back (it is in old/)"; fi;;
    migrated) aside "$path" || die "cannot take $path out";;
  esac
done < "$REV"
rm -f "$REV"
k=1; while [ -e "$J.restored.$k" ]; do k=$((k + 1)); done
mv "$J" "$J.restored.$k" || die "cannot rename the journal"
# old/ holds nothing any more: its empty directories go
find "$OLD" -depth -type d -exec rmdir {} \; 2>/dev/null
sync
say "restored: $n journal entries undone; the renamed files are in $NEW"
