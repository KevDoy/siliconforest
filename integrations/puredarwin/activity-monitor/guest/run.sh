#!/bin/sh
# SPDX-License-Identifier: MIT
# Apple's Activity Monitor from the scratch prefix /private/var/db/sfam (../tools/sf_actmon.sh push),
# the unmodified bundle, with the prefix's libraries in front of the system's
# (docs/roadmap/issues/SF-AA-activity-monitor.md).
P=/private/var/db/sfam; R=$P/root; F=System/Library/Frameworks
export DYLD_FRAMEWORK_PATH=$R/$F:$R/System/Library/PrivateFrameworks
export DYLD_LIBRARY_PATH=$R/usr/lib
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/private/var/run/gpu-desktop} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-wayland-0}
export HOME=${HOME:-/var/root}
exec $CATCH "$P/Activity Monitor.app/Contents/MacOS/Activity Monitor" "$@"
