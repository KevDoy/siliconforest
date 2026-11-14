#!/bin/bash
# SPDX-License-Identifier: MIT
# Regenerates ./0100-*.patch .. (the Activity Monitor patches, docs/roadmap/issues/SF-AA-activity-monitor.md)
# from the patchwork trees in runs/sf-actmon/pw (not committed):
#   base/     = runs/cocoa/patchwork/g + the committed patches 0040..0059 (as committed on 14 November)
#   am-0105/  = base/ + 0100..0105 (frozen)
#   am-0106/  = am-0105/ + 0106 (frozen)
#   am-0107/  = am-0106/ + 0107 (frozen)
#   am-0109/  = am-0107/ + 0108, 0109 (frozen; COCOA3, evening of 14 November)
#   am/       = am-0109/ + 0110 on (the current tree)
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
# 0106: am-0105 -> am-0106
mk am-0105 am-0106 0106-appkit-nib-layout-standard-space-priorities.patch $A/NSLayoutConstraint.m $A/NSView.m $A/NSViewLayout.m $A/NSScrollView.m
( diff -rq am-0105 am-0106 ) | sed 's/^/differs from am-0105: /'
# 0107: am-0106 -> am-0107
mk am-0106 am-0107 0107-appkit-activity-monitor-look-and-cost.patch $A/NSSymbolImages.m $A/NSImage.m $A/nib.subproj/NSCustomResource.m $A/NSImageCell.m \
  $A/NSMenu.subproj/NSMenuView.m $A/NSTableHeaderView.m $A/NSTableView.m $A/NSTableViewViewBased.m $A/NSOutlineView.m \
  $A/NSToolbar.subproj/NSToolbarItemView.m $A/NSToolbar.subproj/NSToolbarView.m $A/NSColor.subproj/NSColor_catalog.h \
  $A/NSColor.subproj/NSColor_catalog.m $A/NSView.m $A/NSPopUpButtonCell.m
( diff -rq am-0106 am-0107 ) | sed 's/^/differs from am-0106: /'
# 0108, 0109: am-0107 -> am-0109
mk am-0107 am-0109 0108-appkit-menu-item-getters-autoreleased.patch $A/NSMenu.subproj/NSMenuItem.m
mk am-0107 am-0109 0109-coregraphics-imageio-thumbnail-metadata.patch src/Frameworks/CoreGraphics/CGImageMetadata.h src/Frameworks/CoreGraphics/CGImageMetadata.m
( diff -rq am-0107 am-0109 ) | sed 's/^/differs from am-0107: /'
# 0110, 0111 on: am-0109 -> am (the current tree)
mk am-0109 am 0110-appkit-collection-view-from-finch.patch $A/NSCollectionView/NSCollectionView.h $A/NSCollectionView/NSCollectionView.m \
  $A/NSCollectionView/NSCollectionViewItem.h $A/NSCollectionView/NSCollectionViewItem.m $A/NSCollectionView/NSCollectionViewLayout.h \
  $A/NSCollectionView/NSCollectionViewLayout.m $A/NSActivityMonitorAdditions.m $A/NSViewController.h $A/NSViewController.m
mk am-0109 am 0111-appkit-catalog-images-symbols-from-finch.patch $A/PDCUIPrivate.h $A/PDCUIRendition.m $A/PDCUICodecs.h \
  $A/PDCUIDeepmap.m $A/PDCUIImage.m $A/NSImage.m $A/PDSymbolGlyphs.m $A/NSSymbolImages.m $A/NSImageSymbolConfiguration.m
mk am-0109 am 0112-appkit-solve-time-trace.patch $A/NSLayoutConstraint.m
mk am-0109 am 0113-appkit-toolbar-buttons-macos26.patch $A/NSButtonCell.m $A/NSToolbar.subproj/NSToolbarView.m \
  $A/NSToolbar.subproj/NSToolbarItemView.m
mk am-0109 am 0114-appkit-overlay-scrollers.patch $A/NSScroller.m $A/NSScrollView.m $A/NSScrollView.h $A/NSView.m \
  $A/NSMacOS26Additions.m $A/NSModernAppKit.m
mk am-0109 am 0115-appkit-cgevent-autoreleased.patch $A/NSEvent.subproj/NSEvent_Modern.m
( diff -rq am-0109 am ) | sed 's/^/differs from am-0109: /'
