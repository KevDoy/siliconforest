#!/bin/bash
# SPDX-License-Identifier: MIT
# Apple's Activity Monitor on SiliconForest: docs/roadmap/issues/SF-AA-activity-monitor.md.
# The method is Terminal.app's (../../terminal-app/tools/sf_terminal.sh); this tool keeps its own
# stage and scratch prefix so the two streams do not share files.
#
#   sf_actmon.sh libs            build ../libs into runs/sf-actmon/libs (against runs/sf-actmon/linkroot:
#                                a copy of Terminal's link root, the target's root with Terminal's tree)
#   sf_actmon.sh stage           runs/sf-actmon/stage/root: the libraries, the test AppKit and
#                                CoreGraphics (runs/sf-actmon/appkit-build, cg-build: ACTMON's cocoa
#                                patches 0100+ over the committed series), the test libSystem
#                                (../build-libsystem-am.nix); probes and run.sh
#   sf_actmon.sh push            the stage and the bundle to /private/var/db/sfam on the target
#   sf_actmon.sh run [seconds] [VAR=value ...]
#                                start it there (DYLD_* to the stage), wait, print the log, stop it (KEEP=1: leave it)
#   sf_actmon.sh scan            the gap table of the application against the stage
#   sf_actmon.sh probe [pid]     the kernel's answers to the calls libsysmon makes (sf-am-probe)
#   sf_actmon.sh inject <item> ...
#                                synthetic input and screen captures through the compositor's inject
#                                module (../../terminal-app/tools/sf_inject.sh, its items); captures
#                                are fetched to runs/sf-actmon/shots/<name>.png. Inside gui.lock only.
# SF_ACTMON_APP: the bundle (default private/mac-apps/m5-26.5.2/Activity Monitor.app; never committed).
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/../../../.." && pwd); cd "$ROOT"
export PATH=$HOME/.nix-profile/bin:$PATH
A=integrations/puredarwin/activity-monitor; W=runs/sf-actmon; ST=$W/stage; F=System/Library/Frameworks
APP=${SF_ACTMON_APP:-private/mac-apps/m5-26.5.2/Activity Monitor.app}
SF_HOST=${SF_HOST:-10.1.1.211}; SF_KNOWN=${SF_KNOWN:-$HOME/.ssh/known_hosts_m1_siliconforest}
SSH=(ssh -o UserKnownHostsFile="$SF_KNOWN" -o GlobalKnownHostsFile=/dev/null -o StrictHostKeyChecking=yes -o HostKeyAlias=m1-siliconforest
     -o PasswordAuthentication=no -o BatchMode=yes -o ConnectTimeout=6 -o ServerAliveInterval=10 "root@$SF_HOST")
TP=/private/var/db/sfam
nb() { (cd "$1" && nix-build --extra-experimental-features 'nix-command flakes' --no-out-link "${@:2}" 2>/dev/null | tail -1); }
toolchain() {
  eval "$(cd integrations/puredarwin/cocoa && nix-instantiate --extra-experimental-features 'nix-command flakes' --eval --strict --json -E 'let a = import ./arm64.nix; in { CC = "${a.common.darwinCrossToolchain}/bin/arm64-apple-darwin20.4-clang"; LD = "${a.common.nativeLd}/bin/ld"; SDK = "${a.common.appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"; I1 = "${a.common.libSystem}/usr/include"; I2 = "${a.base.libobjc}/usr/include"; I3 = "${a.base.corefoundation}/include"; I4 = "${a.base.foundation}/usr/include"; }' 2>/dev/null | python3 -c 'import json,sys; [print("export %s=%s" % kv) for kv in json.load(sys.stdin).items()]')"
  export INC="-I$I1 -I$I2 -I$I3 -I$I4" LINKROOT=$ROOT/$W/linkroot PY=python3 HB=$ROOT/integrations/puredarwin/homebrew
  # the kernel's private headers (<net/ntstat.h> for NetworkStatistics), of the default (tip) kernel
  X=$(nix-build --extra-experimental-features 'nix-command flakes' --no-out-link -E '(import ./integrations/puredarwin/rebase/pd.nix).pd.packages.x86_64-linux.xnu-headers-arm64' 2>/dev/null | tail -1)
  export NTSTAT_INC=$X/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders
}
case "${1:?libs|stage|push|run|scan|probe|inject}" in
libs)
  toolchain; chmod -R u+w $W/libs 2>/dev/null || true; rm -rf $W/libs; mkdir -p $W/libs
  bash $A/libs/build.sh $ROOT/$W/libs > $W/libs.log 2>&1 || { tail -30 $W/libs.log; exit 1; }
  grep -c . $W/libs.log | sed 's/^/files: /';;
stage)
  toolchain
  [ -d $W/libs/usr ] || { echo "run sf_actmon.sh libs first" >&2; exit 2; }
  chmod -R u+w $ST 2>/dev/null || true; rm -rf $ST; mkdir -p $ST/root
  cp -a $W/libs/. $ST/root/
  # the test copies of the drawing frameworks (ACTMON's patches over the committed series)
  mkdir -p $ST/root/$F/AppKit.framework/Versions/A $ST/root/$F/CoreGraphics.framework/Versions/A
  cp $W/appkit-build/AppKit $ST/root/$F/AppKit.framework/Versions/A/AppKit
  ln -sfn A $ST/root/$F/AppKit.framework/Versions/C
  cp $W/cg-build/CoreGraphics $ST/root/$F/CoreGraphics.framework/Versions/A/CoreGraphics
  # the test libSystem (memorystatus_get_level), through DYLD_LIBRARY_PATH only
  LS=$(nb $A build-libsystem-am.nix); cp $LS/usr/lib/libSystem.B.dylib $ST/root/usr/lib/libSystem.B.dylib
  LDF="-fuse-ld=$LD -nostdlib -Wl,-syslibroot,$LINKROOT -L$LINKROOT/usr/lib -Wl,-platform_version,macos,26.5,26.5"
  $CC -isysroot $SDK -mmacosx-version-min=26.5 -O1 -isystem $SDK/usr/include $INC $A/probe/sf-am-probe.c $LDF -lSystem -o $ST/sf-am-probe
  $CC -isysroot $SDK -mmacosx-version-min=26.5 -O1 -fblocks -isystem $SDK/usr/include $INC $A/probe/sf-sysmon-test.c $LDF -L$ROOT/$W/libs/usr/lib -lsysmon -lSystem -o $ST/sf-sysmon-test
  cp $W/tstage/sf-catch $W/tstage/libsfprof.dylib $W/tstage/libsftrace.dylib $ST/ 2>/dev/null || true
  install -m755 $A/guest/run.sh $ST/run.sh
  du -sh $ST | cut -f1;;
push)
  tar -C $ST -cf - . | "${SSH[@]}" "mkdir -p $TP && cd $TP && rm -rf root && /opt/homebrew/bin/gtar -xf - && echo stage pushed"
  if [ -z "${NOAPP:-}" ]; then
    tar -C "$(dirname "$APP")" -cf - "$(basename "$APP")" | "${SSH[@]}" "cd $TP && rm -rf 'Activity Monitor.app' && /opt/homebrew/bin/gtar -xf - && echo app pushed"
  fi;;
run)
  S=${2:-20}; shift; shift || true
  "${SSH[@]}" "cd $TP; pkill -f 'MacOS/Activity Monitor' 2>/dev/null; sleep 1; (env $* CATCH=$TP/sf-catch ./run.sh > out.log 2>&1; echo rc=\$? >> out.log) > /dev/null 2>&1 < /dev/null & sleep $S; ps ax -o pid,rss,%cpu,command | grep 'MacOS/Activity Monitor' | grep -v grep; grep -v 'SFCATCH image' out.log | tail -n ${LINES:-40} | cut -c1-240; [ -n \"${KEEP:-}\" ] || pkill -f 'MacOS/Activity Monitor'" < /dev/null;;
scan)
  python3 -B integrations/puredarwin/homebrew/sf_gapscan.py --root $W/linkroot --overlay $ST/root --overlay $W/tstage/root --overlay $W/scanpd \
    --app "$APP=/System/Applications/Utilities/Activity Monitor.app" --arch arm64e --name ${NAME:-actmon-stage} --out $W/scan;;
probe)
  "${SSH[@]}" "$TP/sf-am-probe ${2:-}" < /dev/null;;
inject)
  shift; SF_INJECT_FETCH=0 integrations/puredarwin/terminal-app/tools/sf_inject.sh "$@"
  mkdir -p $W/shots
  for a in "$@"; do case $a in capture:*) n=${a#capture:}
    "${SSH[@]}" "cat /private/var/run/gpu-desktop/inject/$n.ppm" > "$W/shots/$n.ppm" < /dev/null
    python3 -c "from PIL import Image; import sys; im = Image.open(sys.argv[1]); im.save(sys.argv[2]); print(sys.argv[2], im.size)" "$W/shots/$n.ppm" "$W/shots/$n.png" && rm -f "$W/shots/$n.ppm";;
  esac; done;;
*) echo "libs|stage|push|run|scan|probe|inject" >&2; exit 2;;
esac
