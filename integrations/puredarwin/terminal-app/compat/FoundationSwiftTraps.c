// SPDX-License-Identifier: MIT
// TEMPORARY (docs/roadmap/issues/SF-Y-terminal-app.md, "Swift"): the five Swift entry points
// of Foundation's overlay that libswiftFoundation does not have yet. They exist so that dyld
// can bind the application; a call to one stops the program with its name, which is the truth
// until the overlay has them (Dictionary and NSNumber bridging, Swift sources of the 5.4.3
// overlay). Not to be kept.
#include <stdio.h>
#include <stdlib.h>
#define TRAP(sym) void sym(void) __asm__(#sym); void sym(void) { fprintf(stderr, "SiliconForest: the Swift Foundation overlay has no %s yet\n", #sym); abort(); }
TRAP(_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF)
TRAP(_$sSD10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo12NSDictionaryC_SDyxq_GSgztFZ)
TRAP(_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ)
TRAP(_$sSb10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF)
TRAP(_$sSi10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF)
