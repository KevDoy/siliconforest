#!/bin/bash
# SPDX-License-Identifier: MIT
# Apple's Terminal.app on a running SiliconForest, from a scratch prefix (nothing installed):
# docs/roadmap/issues/SF-Y-terminal-app.md.
#
#   sf_terminal.sh stage          build everything and lay out runs/sf-terminal/stage:
#                                   root/   the libraries the application loads instead of the system's
#                                           (DYLD_FRAMEWORK_PATH / DYLD_LIBRARY_PATH): dyld's and libSystem's
#                                           additions, libFoundation, AppKit, libWindowServer, the compat
#                                           wrappers and private frameworks, ApplicationServices and ImageIO
#                                           of the Cocoa stream, the Swift runtime
#                                   pd/     copies of the real libraries the wrappers re-export (<name>.pd)
#                                   run.sh, show.sh, sf-catch, sf-nibprobe
#   sf_terminal.sh push           send the stage and the application to /private/tmp/sfy on the target
#                                 (the application's arm64e slice, plus a test copy that names
#                                 /tmp/sfy/dyld as its dynamic linker until the system's dyld has the patch)
#   sf_terminal.sh run [seconds] [VAR=value ...]
#                                 start it there, wait, print the summarised log, stop it (KEEP=1: leave it)
#   sf_terminal.sh shot <name>    fetch the window pictures (PD_WS_DUMP_DIR) as runs/sf-terminal/shots/<name>-N.png
#   sf_terminal.sh scan           the gap table of the application against the stage
# SF_TERMINAL_APP: the application bundle (default private/terminal-app/m5-26.5.2/Terminal.app; never committed).
# SF_BASE: a copy of the target's root (default runs/sf-gaps/m1root, with runs/sf-compat/stage laid over it).
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/../../../.." && pwd); cd "$ROOT"
export PATH=$HOME/.nix-profile/bin:$PATH
T=integrations/puredarwin/terminal-app; W=runs/sf-terminal; ST=$W/stage
APP=${SF_TERMINAL_APP:-private/terminal-app/m5-26.5.2/Terminal.app}
BASE=${SF_BASE:-runs/sf-gaps/m1root}; F=System/Library/Frameworks
SF_HOST=${SF_HOST:-10.1.1.211}; SF_KNOWN=${SF_KNOWN:-$HOME/.ssh/known_hosts_m1_siliconforest}
SSH=(ssh -o UserKnownHostsFile="$SF_KNOWN" -o GlobalKnownHostsFile=/dev/null -o StrictHostKeyChecking=yes -o HostKeyAlias=m1-siliconforest
     -o PasswordAuthentication=no -o BatchMode=yes -o ConnectTimeout=6 "root@$SF_HOST")
nb() { (cd "$1" && nix-build --extra-experimental-features 'nix-command flakes' --no-out-link "${@:2}" 2>/dev/null | tail -1); }
WRAPPED="CoreGraphics CoreText QuartzCore Carbon CoreServices AudioToolbox"
case "${1:?stage|push|run|shot|scan}" in
stage)
  LS=$(nb $T build-libsystem.nix); FN=$(nb $T build-foundation.nix); SW=$(nb $T build-swift-runtime.nix)
  AK=$(nb integrations/puredarwin/cocoa patched.nix -A appkit); WS=$(nb integrations/puredarwin/cocoa patched.nix -A windowserver)
  FF=$(nb integrations/puredarwin/cocoa build-firefox-root.nix)
  for v in LS FN SW AK WS FF; do [ -n "${!v}" ] || { echo "a build failed ($v)" >&2; exit 2; }; done
  eval "$(cd integrations/puredarwin/cocoa && nix-instantiate --extra-experimental-features 'nix-command flakes' --eval --strict --json -E 'let a = import ./arm64.nix; in { CC = "${a.common.darwinCrossToolchain}/bin/arm64-apple-darwin20.4-clang"; LD = "${a.common.nativeLd}/bin/ld"; SDK = "${a.common.appleSdk}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"; I1 = "${a.common.libSystem}/usr/include"; I2 = "${a.base.libobjc}/usr/include"; I3 = "${a.base.corefoundation}/include"; I4 = "${a.base.foundation}/usr/include"; }' 2>/dev/null | python3 -c 'import json,sys; [print("export %s=%s" % kv) for kv in json.load(sys.stdin).items()]')"
  export INC="-I$I1 -I$I2 -I$I3 -I$I4" PY=python3 HB=$ROOT/integrations/puredarwin/homebrew
  # the link root: the target's root as it is, with this job's libSystem, Foundation and Swift runtime
  chmod -R u+w "$W/linkroot" "$ST" 2>/dev/null || true; rm -rf "$ROOT/$W/linkroot" "$ROOT/$ST"; mkdir -p $W/linkroot $ST/root/usr/lib/swift $ST/pd
  cp -a --reflink=auto $BASE/usr $BASE/System $BASE/lib $W/linkroot/; chmod -R u+w $W/linkroot
  [ -d runs/sf-compat/stage/new ] && cp -a --reflink=auto runs/sf-compat/stage/new/. $W/linkroot/ && chmod -R u+w $W/linkroot
  [ -f runs/sf-compat/stage/replace/CoreServices.new ] && cp runs/sf-compat/stage/replace/CoreServices.new $W/linkroot/$F/CoreServices.framework/Versions/A/CoreServices
  cp $LS/usr/lib/libSystem.B.dylib $W/linkroot/usr/lib/; cp $FN/usr/lib/libFoundation.dylib $W/linkroot/usr/lib/
  mkdir -p $W/linkroot/usr/lib/swift; cp $SW/usr/lib/swift/*.dylib $W/linkroot/usr/lib/swift/; chmod -R u+w $W/linkroot
  export LINKROOT=$ROOT/$W/linkroot
  bash $T/compat/build.sh $ROOT/$ST/root > $W/compat-build.log 2>&1 || { tail -5 $W/compat-build.log >&2; exit 2; }
  for n in $WRAPPED; do cp $W/linkroot/$F/$n.framework/Versions/A/$n $ST/pd/$n.pd; done
  # AppKit: the binary and its resources (NSBundle finds them beside it); the headers stay behind
  mkdir -p $ST/root/$F/AppKit.framework/Versions/A $ST/root/$F/AppKit.framework/Versions/C
  cp $AK/$F/AppKit.framework/Versions/A/AppKit $ST/root/$F/AppKit.framework/Versions/A/
  cp -a $AK/$F/AppKit.framework/Versions/A/Resources $ST/root/$F/AppKit.framework/Versions/A/
  chmod -R u+w $ST; ln -sfn ../A/AppKit $ST/root/$F/AppKit.framework/Versions/C/AppKit
  ln -sfn Versions/A/Resources $ST/root/$F/AppKit.framework/Resources
  cp -a $FF/$F/ApplicationServices.framework $FF/$F/ImageIO.framework $ST/root/$F/
  cp $LS/usr/lib/libSystem.B.dylib $LS/usr/lib/dyld $FN/usr/lib/libFoundation.dylib $WS/usr/lib/libWindowServer.dylib $ST/root/usr/lib/
  cp $SW/usr/lib/swift/*.dylib $ST/root/usr/lib/swift/; chmod -R u+w $ST
  CCF="-isysroot $SDK -mmacosx-version-min=26.5 -O1 -fno-objc-arc -fobjc-exceptions -fblocks -Wno-nullability-completeness -Wno-objc-root-class -Wno-objc-method-access $INC"
  LDF="-fuse-ld=$LD -nostdlib -Wl,-syslibroot,$LINKROOT -L$LINKROOT/usr/lib -Wl,-platform_version,macos,26.5,26.5"
  $CC -std=gnu11 -O1 -isystem $SDK/usr/include $T/tools/sf-catch.c $LDF -lSystem -o $ST/sf-catch 2>/dev/null
  $CC $CCF $T/probe/sf-nibprobe.m $LDF -lobjc -lFoundation -lCoreFoundation -lSystem -o $ST/sf-nibprobe 2>/dev/null
  install -m755 $T/guest/run.sh $T/guest/show.sh $T/guest/probe.sh $ST/
  # the test copy: the arm64e slice with /tmp/sfy/dyld (same length) as its dynamic linker
  python3 -B - "$APP" "$ST/Terminal.testdyld" <<'PYEOF'
import plistlib, sys
sys.path.insert(0, 'integrations/puredarwin/homebrew')
import hb_macho
app, out = sys.argv[1:3]
exe = app + '/Contents/MacOS/' + plistlib.load(open(app + '/Contents/Info.plist', 'rb'))['CFBundleExecutable']
m = hb_macho.load(exe, 'arm64e')
assert m.data.count(b'/usr/lib/dyld\0') == 1
open(out, 'wb').write(m.data.replace(b'/usr/lib/dyld\0', b'/tmp/sfy/dyld\0'))
PYEOF
  du -sh $ST | cut -f1;;
push)
  tar -C $ST -cf - . | "${SSH[@]}" 'mkdir -p /private/tmp/sfy/shots && cd /private/tmp/sfy && /opt/homebrew/bin/gtar -xf - && cp root/usr/lib/dyld dyld && chmod 755 dyld Terminal.testdyld run.sh show.sh probe.sh sf-catch sf-nibprobe'
  tar -C "$(dirname "$APP")" -cf - "$(basename "$APP")" | "${SSH[@]}" 'cd /private/tmp/sfy && /opt/homebrew/bin/gtar -xf - && cp Terminal.testdyld Terminal.app/Contents/MacOS/Terminal.testdyld && echo pushed';;
run)
  S=${2:-20}; shift; shift || true
  "${SSH[@]}" "cd /private/tmp/sfy; pkill -f 'MacOS/Terminal.testdyld' 2>/dev/null; sleep 1; rm -f shots/*; (env PD_WS_DUMP_DIR=/private/tmp/sfy/shots $* CATCH=/private/tmp/sfy/sf-catch ./run.sh > out.log 2>&1; echo rc=\$? >> out.log) > /dev/null 2>&1 < /dev/null & sleep $S; cd /private/tmp/sfy; ./show.sh; [ -n \"${KEEP:-}\" ] || pkill -f 'MacOS/Terminal.testdyld'" < /dev/null;;
shot)
  N=${2:?name}; mkdir -p $W/shots; i=0
  for f in $("${SSH[@]}" 'ls /private/tmp/sfy/shots/*.ppm 2>/dev/null' < /dev/null); do
    i=$((i + 1)); "${SSH[@]}" "cat $f" < /dev/null > $W/shots/$N-$i.ppm
    python3 -c "from PIL import Image; import sys; im = Image.open(sys.argv[1]); im.save(sys.argv[2]); print(sys.argv[2], im.size)" $W/shots/$N-$i.ppm $W/shots/$N-$i.png
  done;;
scan)
  mkdir -p $W/scanpd; for n in $WRAPPED; do mkdir -p $W/scanpd/$F/$n.framework/Versions/A; cp $ST/pd/$n.pd $W/scanpd/$F/$n.framework/Versions/A/$n.pd; done
  python3 -B integrations/puredarwin/homebrew/sf_gapscan.py --root $W/linkroot --overlay $ST/root --overlay $W/scanpd \
    --app "$APP=/Applications/Utilities/Terminal.app" --arch arm64e --name terminal-stage;;
esac
