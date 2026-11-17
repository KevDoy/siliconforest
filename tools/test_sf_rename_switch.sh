#!/bin/bash
# SPDX-License-Identifier: MIT
# Host test of tools/sf_rename_switch.sh and tools/sf_rename_restore.sh against a mock root.
#   test_sf_rename_switch.sh [SHELL]        SHELL runs the two scripts (default sh)
#   test_sf_rename_switch.sh [SHELL] --stage STAGE --mock ROOT
#       the same cases with a real stage and a root prepared for it (tools/sf_rename_stage.py;
#       ROOT is copied, never changed)
# Cases: a dry run changes nothing; a switch gives the new hashes and keeps every old file; a
# second switch does nothing; a restore gives the first root back byte for byte; a switch
# stopped after every possible number of changes restores to the first root, and run again
# completes; a live file that differs from the record stops the switch before any change;
# classes done in two runs; core files are never absent from their path.
set -euo pipefail
HERE=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SH=sh; STAGE=; MOCK=
while [ $# -gt 0 ]; do case "$1" in --stage) STAGE=$2; shift;; --mock) MOCK=$2; shift;; *) SH=$1;; esac; shift; done
W=$(mktemp -d); trap 'chmod -R u+w "$W" 2>/dev/null; rm -rf "$W"' EXIT
sw() { $SH "$HERE/sf_rename_switch.sh" "$@"; }
rs() { $SH "$HERE/sf_rename_restore.sh" "$@"; }
T=$(printf '\t'); pass=0; fail=0
ok() { pass=$((pass + 1)); echo "ok   $*"; }
no() { fail=$((fail + 1)); echo "FAIL $*"; }
# every file and link of a root except the switch's own directory: path, kind, content hash or target
snap() { (cd "$1" && find . -path ./private/var/db/sf-rename -prune -o \( -type f -o -type l \) -print | LC_ALL=C sort | while read -r f; do
  if [ -L "$f" ]; then echo "$f link $(readlink "$f")"; else echo "$f file $(sha256sum "$f" | cut -c1-64) $(stat -c %a "$f")"; fi; done); }
if [ -z "$STAGE" ]; then
  # ---- a small stage with every kind of row ----
  STAGE=$W/stage; MOCK=$W/mock; mkdir -p $STAGE/root/usr/lib $STAGE/root/usr/sbin $STAGE/root/System/Library/LaunchDaemons $STAGE/root/usr/share/doc/siliconforest
  mkdir -p $MOCK/usr/lib $MOCK/usr/sbin $MOCK/System/Library/LaunchDaemons $MOCK/Library/Preferences $MOCK/etc $MOCK/private/var/db
  echo new-libsystem > $STAGE/root/usr/lib/libSystem.B.dylib; echo old-libsystem > $MOCK/usr/lib/libSystem.B.dylib
  echo new-dyld > $STAGE/root/usr/lib/dyld; echo old-dyld > $MOCK/usr/lib/dyld
  echo new-netd > $STAGE/root/usr/sbin/sf-netd; echo old-netd > $MOCK/usr/sbin/pd-netd
  echo new-job > $STAGE/root/System/Library/LaunchDaemons/org.siliconforest.sf-netd.plist; echo old-job > $MOCK/System/Library/LaunchDaemons/org.puredarwin.pd-netd.plist
  echo notice > $STAGE/root/usr/share/doc/siliconforest/NOTICE
  echo new-appkit > $STAGE/root/usr/lib/libAppKit.dylib; echo locally-changed-appkit > $MOCK/usr/lib/libAppKit.dylib
  echo networks > $MOCK/Library/Preferences/org.puredarwin.wifi.known
  printf '<?xml version="1.0"?>\n<plist><dict><key>PDAudio:speakers</key><real>0.4</real></dict></plist>\n' > $MOCK/Library/Preferences/org.puredarwin.audio.plist
  ln -s libOld.dylib $MOCK/usr/lib/libLink.dylib; echo old-target > $MOCK/usr/lib/libOld.dylib
  echo hosts-already-new > $MOCK/etc/hosts; mkdir -p $STAGE/root/etc; echo hosts-already-new > $STAGE/root/etc/hosts
  h() { sha256sum "$1" | cut -c1-64; }
  { echo "# test manifest"
    echo "install$T/usr/lib/libSystem.B.dylib$T$(h $MOCK/usr/lib/libSystem.B.dylib)$T$(h $STAGE/root/usr/lib/libSystem.B.dylib)${T}0755${T}core${T}t"
    echo "install$T/usr/lib/dyld$T$(h $MOCK/usr/lib/dyld)$T$(h $STAGE/root/usr/lib/dyld)${T}0755${T}core${T}t"
    echo "install$T/usr/sbin/sf-netd${T}absent$T$(h $STAGE/root/usr/sbin/sf-netd)${T}0755${T}direct${T}t"
    echo "remove$T/usr/sbin/pd-netd$T$(h $MOCK/usr/sbin/pd-netd)$T-$T-${T}direct${T}t"
    echo "install$T/usr/lib/libAppKit.dylib${T}unknown$T$(h $STAGE/root/usr/lib/libAppKit.dylib)${T}0644${T}direct${T}t"
    echo "install$T/etc/hosts${T}unknown$T$(h $STAGE/root/etc/hosts)${T}0644${T}direct${T}already done by hand"
    echo "link$T/usr/lib/libLink.dylib${T}link:libOld.dylib${T}libSystem.B.dylib$T-${T}direct${T}t"
    echo "install$T/System/Library/LaunchDaemons/org.siliconforest.sf-netd.plist${T}absent$T$(h $STAGE/root/System/Library/LaunchDaemons/org.siliconforest.sf-netd.plist)${T}0644${T}launchd${T}t"
    echo "remove$T/System/Library/LaunchDaemons/org.puredarwin.pd-netd.plist${T}unknown$T-$T-${T}launchd${T}t"
    echo "install$T/usr/share/doc/siliconforest/NOTICE${T}absent$T$(h $STAGE/root/usr/share/doc/siliconforest/NOTICE)${T}0644${T}direct${T}t"
    echo "migrate$T/Library/Preferences/org.siliconforest.wifi.known${T}unknown$T-${T}0644${T}state${T}from=/Library/Preferences/org.puredarwin.wifi.known"
    echo "migrate$T/Library/Preferences/org.siliconforest.audio.plist${T}unknown$T-${T}0644${T}state${T}from=/Library/Preferences/org.puredarwin.audio.plist;sed=s/PDAudio:/SFAudio:/g"
    echo "migrate$T/Library/Preferences/org.siliconforest.none${T}unknown$T-${T}0644${T}state${T}from=/Library/Preferences/org.puredarwin.none"
    echo "keep$T/usr/libexec/pd-set-hostname.pre-sf${T}unknown$T-$T-${T}none${T}kept by an earlier hand change"
  } > $STAGE/manifest.tsv
fi
fresh() { chmod -R u+w "$W/r" 2>/dev/null || true; rm -rf "$W/r"; cp -a "$MOCK" "$W/r"; }
fresh; snap $W/r > $W/before
NROWS=$(grep -v '^#' $STAGE/manifest.tsv | grep -c -v "^keep$T")

# 1. dry run
sw --dry-run --root $W/r $STAGE > $W/dry.out 2>&1 && rc=0 || rc=$?
snap $W/r > $W/now; { [ $rc = 0 ] && cmp -s $W/before $W/now && [ ! -e $W/r/private/var/db/sf-rename ]; } && ok "dry run: rc 0, root unchanged, nothing written ($(grep -c . $W/dry.out) lines)" || { no "dry run rc=$rc"; tail -5 $W/dry.out; }
rs --dry-run --root $W/r > $W/o 2>&1 && no "restore dry run without a journal should fail" || ok "restore without a journal refuses"

# 2. switch
sw --root $W/r $STAGE > $W/sw.out 2>&1 && rc=0 || rc=$?
bad=0
while IFS=$T read -r act path old new mode class note; do
  case "$act" in
    install) [ "$(sha256sum "$W/r$path" | cut -c1-64)" = "$new" ] || { echo "  not new: $path"; bad=1; }
             case "$old" in absent|unknown) ;; *) [ "$(sha256sum "$W/r/private/var/db/sf-rename/old$path" 2>/dev/null | cut -c1-64)" = "$old" ] || [ "$old" = "$new" ] || { echo "  old not kept: $path"; bad=1; };; esac;;
    link) [ "$(readlink "$W/r$path")" = "$new" ] || { echo "  not linked: $path"; bad=1; };;
    remove) [ ! -e "$W/r$path" ] || { echo "  still there: $path"; bad=1; };;
    migrate) from=${note#from=}; case "$from" in *";sed="*) [ ! -e "$MOCK${from%%;sed=*}" ] || grep -q 'SFAudio:speakers' "$W/r$path" || { echo "  names not rewritten: $path"; bad=1; }; continue;; esac; if [ -e "$MOCK$from" ] && [ ! -e "$MOCK$path" ]; then cmp -s "$W/r$from" "$W/r$path" || { echo "  not carried over: $path"; bad=1; }; cmp -s "$MOCK$from" "$W/r$from" || { echo "  source changed: $from"; bad=1; }; fi;;
  esac
done < <(grep -v '^#' $STAGE/manifest.tsv)
{ [ $rc = 0 ] && [ $bad = 0 ]; } && ok "switch: every row as the manifest says, old files kept ($(tail -1 $W/sw.out))" || { no "switch rc=$rc"; tail -5 $W/sw.out; }
snap $W/r > $W/after
# 3. again: nothing to do
sw --root $W/r $STAGE > $W/sw2.out 2>&1 && rc=0 || rc=$?
snap $W/r > $W/now; { [ $rc = 0 ] && cmp -s $W/after $W/now && grep -q 'switched: 0 changes' $W/sw2.out; } && ok "second switch: 0 changes" || { no "second switch rc=$rc"; tail -3 $W/sw2.out; }
# 4. restore
rs --dry-run --root $W/r > $W/rd.out 2>&1 && rc=0 || rc=$?
snap $W/r > $W/now; { [ $rc = 0 ] && cmp -s $W/after $W/now; } && ok "restore dry run: rc 0, nothing changed" || no "restore dry run rc=$rc"
rs --root $W/r > $W/rs.out 2>&1 && rc=0 || rc=$?
snap $W/r > $W/now; { [ $rc = 0 ] && cmp -s $W/before $W/now; } && ok "restore: the root is the first root byte for byte ($(tail -1 $W/rs.out))" || { no "restore rc=$rc"; diff $W/before $W/now | head -5; tail -3 $W/rs.out; }
[ -z "$(find $W/r/private/var/db/sf-rename/old -type f 2>/dev/null)" ] && ok "restore: old/ is empty" || no "old/ still holds files"
# 5. switch after a restore
sw --root $W/r $STAGE > $W/o 2>&1 && snap $W/r > $W/now && cmp -s $W/after $W/now && ok "switch after restore: same result" || no "switch after restore"
# 6. interrupted after every possible number of changes: restore gives the first root; run again completes
ibad=0; n=1
while [ $n -lt $NROWS ]; do
  fresh
  sw --root $W/r --fail-after $n $STAGE > $W/o 2>&1 && break      # fewer changes than n: done
  cp -a $W/r $W/r2
  rs --root $W/r > $W/o 2>&1 || { echo "  restore failed after $n"; tail -2 $W/o; ibad=1; }
  snap $W/r > $W/now; cmp -s $W/before $W/now || { echo "  after $n changes + restore: root differs"; diff $W/before $W/now | head -3; ibad=1; }
  chmod -R u+w $W/r; rm -rf $W/r; mv $W/r2 $W/r
  sw --root $W/r $STAGE > $W/o 2>&1 || { echo "  resume failed after $n"; tail -2 $W/o; ibad=1; }
  snap $W/r > $W/now; cmp -s $W/after $W/now || { echo "  after $n changes + resume: not the switched root"; ibad=1; }
  rs --root $W/r > $W/o 2>&1; snap $W/r > $W/now; cmp -s $W/before $W/now || { echo "  resumed switch + restore: root differs after $n"; ibad=1; }
  n=$((n + 1))
done
[ $ibad = 0 ] && ok "interrupted after 1..$((n - 1)) changes: restore gives the first root; resumed switch completes and restores" || no "interrupted switch"
# 7. a live file that differs from the record
fresh
first=$(grep -v '^#' $STAGE/manifest.tsv | awk -F'\t' '$1=="install" && $3 ~ /^[0-9a-f]{64}$/ {print $2; exit}')
if [ -n "$first" ]; then
  chmod u+w "$W/r$first"; echo changed-by-hand >> "$W/r$first"; snap $W/r > $W/b7
  sw --root $W/r $STAGE > $W/o 2>&1 && rc=0 || rc=$?
  snap $W/r > $W/now; { [ $rc != 0 ] && cmp -s $W/b7 $W/now && grep -q "^DIFF $first" $W/o; } && ok "a live file differing from the record stops the switch before any change" || no "mismatch not refused (rc=$rc)"
  sw --dry-run --root $W/r $STAGE > $W/o 2>&1 && rc=0 || rc=$?; [ $rc = 3 ] && ok "dry run reports the difference with rc 3" || no "dry run with a difference rc=$rc"
  sw --allow-mismatch --root $W/r $STAGE > $W/o 2>&1 && rs --root $W/r > $W/o 2>&1 && snap $W/r > $W/now && cmp -s $W/b7 $W/now && ok "--allow-mismatch switches, keeps the changed file, restore gives it back" || no "--allow-mismatch"
fi
# 8. classes in two runs
fresh
sw --root $W/r --classes "state direct launchd" $STAGE > $W/o 2>&1 && rc=0 || rc=$?
corebad=0; while IFS=$T read -r act path old new mode class note; do [ "$class" = core ] && [ "$act" = install ] && [ "$old" != unknown ] && [ "$old" != absent ] && { [ "$(sha256sum "$W/r$path" | cut -c1-64)" = "$old" ] || corebad=1; }; done < <(grep -v '^#' $STAGE/manifest.tsv)
{ [ $rc = 0 ] && [ $corebad = 0 ]; } && ok "--classes without core leaves the core files" || no "--classes (rc=$rc)"
sw --root $W/r --classes core $STAGE > $W/o 2>&1 && snap $W/r > $W/now && cmp -s $W/after $W/now && ok "the core class in a second run completes the switch" || no "core in a second run"
rs --root $W/r > $W/o 2>&1 && snap $W/r > $W/now && cmp -s $W/before $W/now && ok "restore after two runs: first root" || no "restore after two runs"
# 9. --name keeps a separate journal
fresh
sw --root $W/r --name sf-rename-audio-pretest $STAGE > $W/o 2>&1 && [ -f $W/r/private/var/db/sf-rename-audio-pretest/journal ] && [ ! -e $W/r/private/var/db/sf-rename ] \
  && rs --root $W/r --name sf-rename-audio-pretest > $W/o 2>&1 && ok "--name: own directory, switch and restore" || no "--name"
echo "passed $pass, failed $fail (shell: $SH)"
[ $fail = 0 ]
