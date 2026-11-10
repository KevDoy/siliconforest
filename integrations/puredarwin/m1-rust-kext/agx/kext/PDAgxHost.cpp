// SPDX-License-Identifier: GPL-2.0
// PDAgxHost.cpp - finds the PDAgx IOService from the nub the platform bus was given. The
// IOKit class itself is the tree's fbsdkpi/xnu/PDDRMPlatformDevice.cpp, unchanged; the RTKit
// port needs that IOService (pd_rtkit_platform_init) and the LinuxKPI side only has the nub.

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSIterator.h>

extern "C" void *
pd_agx_host_of_nub(void *nub)
{
	IOService *provider = OSDynamicCast(IOService, (OSObject *)nub);
	IOService *found = NULL;

	if (provider == NULL)
		return NULL;
	OSIterator *it = provider->getClientIterator();
	if (it == NULL)
		return NULL;
	while (OSObject *o = it->getNextObject()) {
		IOService *client = OSDynamicCast(IOService, o);
		// start() of the kext's class is what is running; it is attached already
		if (client != NULL && client->metaCast("PDAgx") != NULL) {
			found = client;
			break;
		}
	}
	it->release();
	return found;
}
