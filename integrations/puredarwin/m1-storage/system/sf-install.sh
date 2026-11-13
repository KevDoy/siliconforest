#!/bin/sh
# SPDX-License-Identifier: MIT
# Put the disk root's own system tools into a SiliconForest root. Runs inside the booted
# system (the files come from the RAM root image, the root is written through the fenced
# driver) or on any host against a mounted or unpacked root. Idempotent.
#   sf-install.sh <directory with sf-halt sf-clock sf-boot sf-command darwin-desktop-test.disk> <root directory>
set -eu
S=${1:?source directory}; R=${2:?root directory}
[ -x "$R/sbin/launchd" ] && [ -d "$R/System" ] && [ -d "$R/private" ] || { echo "SF_INSTALL not a SiliconForest root: $R"; exit 2; }
for f in sf-halt sf-clock sf-boot sf-command darwin-desktop-test.disk; do [ -r "$S/$f" ] || { echo "SF_INSTALL missing $S/$f"; exit 2; }; done
put() {  # <source> <destination>: by way of a new name, so a running program's file is replaced, never rewritten
    cp "$S/$1" "$R/$2.sf-new" && chmod 755 "$R/$2.sf-new" && mv -f "$R/$2.sf-new" "$R/$2"
}
mkdir -p "$R/usr/libexec" "$R/private/var/run" "$R/private/tmp" "$R/private/ram"
put sf-halt sbin/sf-halt
put sf-clock usr/sbin/sf-clock
put sf-boot sbin/sf-boot
put darwin-desktop-test.disk usr/bin/darwin-desktop-test
for c in reboot halt poweroff shutdown; do
    if [ -e "$R/bin/$c" ] && ! grep -q '/sbin/sf-halt' "$R/bin/$c" 2>/dev/null; then mv -f "$R/bin/$c" "$R/bin/$c.xnu"; fi
    put sf-command "bin/$c"
done
echo "SF_INSTALL done: $(cd "$R" && ls -l sbin/sf-halt sbin/sf-boot usr/sbin/sf-clock bin/reboot bin/halt bin/poweroff bin/shutdown usr/bin/darwin-desktop-test 2>&1 | awk '{print $1 ":" $5 ":" $NF}' | tr '\n' ' ')"
