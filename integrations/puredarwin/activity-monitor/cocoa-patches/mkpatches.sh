#!/bin/bash
# SPDX-License-Identifier: MIT
# Regenerates ./0100-*.patch .. from ACTMON's patchwork trees (runs/sf-actmon/pw, not committed):
#   base/ = runs/cocoa/patchwork/g + the committed patches 0040..0050 (TERM's series as committed),
#   am/   = base/ + Activity Monitor's changes. Each patch diffs base/ against am/ for its files; a file
# belongs to one patch. (TERM's patches 0051..0099 are folded in by rebasing these over them:
# docs/roadmap/issues/SF-AA-activity-monitor.md, "Cocoa patches".)
cd /home/chrisk/src/darwin-lab/runs/sf-actmon/pw; P=/home/chrisk/src/darwin-lab/integrations/puredarwin/activity-monitor/cocoa-patches
mk(){ out=$1; shift; : > $P/$out; for f in "$@"; do diff -uN --label a/$f --label b/$f base/$f am/$f >> $P/$out; done; echo "$out $(wc -l < $P/$out)"; }
A=src/Frameworks/AppKit
mk 0100-appkit-activity-monitor-names.patch $A/NSActivityMonitorAdditions.m
mk 0101-appkit-view-based-tables-hidden-columns.patch $A/NSTableViewViewBased.m $A/NSTableView.m $A/NSOutlineView.m $A/NSTableColumn.h $A/NSTableColumn.m $A/NSTableHeaderView.m $A/NSLayoutConstraint.m
mk 0103-appkit-layer-backed-views-without-egl.patch $A/NSView.m
mk 0104-appkit-found-with-activity-monitor.patch $A/nib.subproj/NSNibArchiveUnarchiver.m $A/nib.subproj/NSCustomView.m $A/NSCell.m $A/NSControl.m $A/NSKeyValueBinding/NSBinder.m $A/NSKeyValueBinding/NSKVOBinder.m $A/NSKeyValueBinding/NSCachingBinder.m $A/NSKeyValueBinding/NSMultipleValueBinder.m $A/NSWindowMenuStrip.m
mk 0105-appkit-asset-catalog-colors-from-finch.patch $A/PDCUIPrivate.h $A/PDCUIStore.m $A/PDCUIRendition.m
mk 0102-coregraphics-activity-monitor-cgs.patch src/Frameworks/CoreGraphics/CGActivityMonitorAdditions.m
# every file that differs between base/ and am/ must be named above
diff -rq base am | sed 's/^/differs: /'
