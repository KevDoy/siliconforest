#!/bin/bash
# SPDX-License-Identifier: MIT
# Regenerates ./0100-*.patch .. (the Activity Monitor patches, docs/roadmap/issues/SF-AA-activity-monitor.md)
# from the patchwork trees in runs/sf-actmon/pw (not committed):
#   base/     = runs/cocoa/patchwork/g + the committed patches 0040..0059 (as committed on 14 November)
#   am-0105/  = base/ + 0100..0105 (frozen)
#   am/       = am-0105/ + 0106 on (the current tree)
# Each patch diffs two trees for its files; a file belongs to one patch of a set. They apply after 0059
# (../patched.nix).
cd /home/chrisk/src/darwin-lab/runs/sf-actmon/pw; P=/home/chrisk/src/darwin-lab/integrations/puredarwin/cocoa/patches
mk(){ from=$1 to=$2 out=$3; shift 3; : > $P/$out; for f in "$@"; do diff -uN --label a/$f --label b/$f $from/$f $to/$f >> $P/$out; done; echo "$out $(wc -l < $P/$out)"; }
A=src/Frameworks/AppKit
mk base am-0105 0100-appkit-activity-monitor-names.patch $A/NSActivityMonitorAdditions.m
mk base am-0105 0101-appkit-view-based-tables-hidden-columns.patch $A/NSTableViewViewBased.m $A/NSTableView.m $A/NSOutlineView.m $A/NSTableColumn.h $A/NSTableColumn.m $A/NSTableHeaderView.m $A/NSLayoutConstraint.m
mk base am-0105 0103-appkit-layer-backed-views-without-egl.patch $A/NSView.m
mk base am-0105 0104-appkit-found-with-activity-monitor.patch $A/nib.subproj/NSNibArchiveUnarchiver.m $A/nib.subproj/NSCustomView.m $A/NSCell.m $A/NSControl.m $A/NSKeyValueBinding/NSBinder.m $A/NSKeyValueBinding/NSKVOBinder.m $A/NSKeyValueBinding/NSCachingBinder.m $A/NSKeyValueBinding/NSMultipleValueBinder.m $A/NSWindowMenuStrip.m
mk base am-0105 0105-appkit-asset-catalog-colors-from-finch.patch $A/PDCUIPrivate.h $A/PDCUIStore.m $A/PDCUIRendition.m
mk base am-0105 0102-coregraphics-activity-monitor-cgs.patch src/Frameworks/CoreGraphics/CGActivityMonitorAdditions.m
( diff -rq base am-0105 ) | sed 's/^/differs from base: /'
# 0106 on: am-0105 -> am
mk am-0105 am 0106-appkit-nib-layout-standard-space-priorities.patch $A/NSLayoutConstraint.m $A/NSView.m $A/NSViewLayout.m $A/NSScrollView.m
( diff -rq am-0105 am ) | sed 's/^/differs from am-0105: /'
