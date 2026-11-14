// SPDX-License-Identifier: MIT
// NetworkStatistics.framework (private) for Apple's Activity Monitor on SiliconForest
// (docs/roadmap/issues/SF-AA-activity-monitor.md): the client of the kernel's ntstat control
// that reports per-socket byte counts. Not implemented yet: a manager is made and accepts its
// requests, but it never reports a source, so the Network tab's per-process columns stay empty.
// The system-wide network totals come from libsysmon (interface counters), not from here.
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>

struct sf_nstat_manager { dispatch_queue_t queue; void *block; };
typedef struct sf_nstat_manager *NStatManagerRef;
typedef void *NStatSourceRef;

const CFStringRef kNStatSrcKeyInterface = CFSTR("interface");
const CFStringRef kNStatSrcKeyPID = CFSTR("processID");
const CFStringRef kNStatSrcKeyRxBytes = CFSTR("rxBytes");
const CFStringRef kNStatSrcKeyRxPackets = CFSTR("rxPackets");
const CFStringRef kNStatSrcKeyTxBytes = CFSTR("txBytes");
const CFStringRef kNStatSrcKeyTxPackets = CFSTR("txPackets");

static void said(void)
{
	static int once;
	if (!once++)
		fprintf(stderr, "NetworkStatistics: no per-socket statistics on this system (no ntstat client): no sources\n");
}

NStatManagerRef NStatManagerCreate(CFAllocatorRef allocator, dispatch_queue_t queue, void *block)
{
	(void)allocator;
	struct sf_nstat_manager *m = calloc(1, sizeof(*m));
	if (m) { m->queue = queue; m->block = block; }
	said();
	return m;
}

void NStatManagerDestroy(NStatManagerRef m) { free(m); }
int NStatManagerAddAllTCP(NStatManagerRef m) { (void)m; return 0; }
int NStatManagerAddAllUDP(NStatManagerRef m) { (void)m; return 0; }

// no sources: the update is complete at once; a completion block, if given, is called as macOS does
int NStatManagerQueryAllSourcesUpdate(NStatManagerRef m, void (^done)(void))
{
	if (done && m && m->queue)
		dispatch_async(m->queue, done);
	return 0;
}

CFDictionaryRef NStatSourceCopyCounts(NStatSourceRef s) { (void)s; return NULL; }
CFDictionaryRef NStatSourceCopyProperties(NStatSourceRef s) { (void)s; return NULL; }
void NStatSourceSetDescriptionBlock(NStatSourceRef s, void *b) { (void)s; (void)b; }
void NStatSourceSetRemovedBlock(NStatSourceRef s, void *b) { (void)s; (void)b; }
