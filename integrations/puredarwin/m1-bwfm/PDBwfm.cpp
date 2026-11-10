// SPDX-License-Identifier: MIT
// Broadcom BCM4378 PCIe FullMAC 802.11ax (14e4:4425, Apple M1 J293), OpenBSD's bwfm(4) on
// IO80211Family, with the pci calls it makes. Derived from PDQwx.cpp. The interrupt is a
// poll timer and DMA goes through the DART bounce pool, see pd_bwfm_compat.h.

#include "pd_dart_pool.h"
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOInterruptController.h>
#include "IO80211Controller.h"
#include "pd_bwfm_glue.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <pexpert/pexpert.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/kpi_mbuf.h>

#include "pd_bwfm_compat.h"

extern "C" {
void *pd80211_malloc(size_t, int, int);
void pd80211_free(void *, int, size_t);
void pd80211_lock(void);
void pd80211_unlock(void);
vnode_t vfs_rootvnode(void);
}

#define LOG(fmt, ...)		IOLog(PD_BWFM_NAME ": " fmt "\n", ##__VA_ARGS__)

#define BWFM_ROOT_WAIT_S	300
#define BWFM_MAX_BARS		2
#define BWFM_MAX_DEVICES	2

class PDBwfm;

struct pd_bwfm_dma {
	IOPCIDevice	*pci;
};

class PDBwfm : public IO80211Controller
{
	OSDeclareDefaultStructors(PDBwfm)

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual const OSString *newVendorString() const APPLE_KEXT_OVERRIDE;
	virtual const OSString *newModelString() const APPLE_KEXT_OVERRIDE;

	IOPCIDevice *fPCI;
	IOWorkLoop *fPollLoop;
	IOTimerEventSource *fPollTimer;
	IOInterruptEventSource *fIntSrc;
	UInt32 fPollUS;
	volatile UInt32 fIntrCount;
	bool setupMSI();
	static void intrAction(OSObject *owner, IOInterruptEventSource *sender, int count);
	IOMemoryMap *fBars[BWFM_MAX_BARS];
	int (*fIntrFn)(void *);
	void *fIntrArg;
	volatile bool fPolling;
	static void pollAction(OSObject *owner, IOTimerEventSource *sender);

private:
	static void firmwareThread(void *arg, wait_result_t wr);
	void bringUp();
	void teardown();

	struct pd_bwfm_dma fDma;
	struct device *fDev;
	bool fAttached;
	bool f80211;
	volatile bool fStopping;
	volatile bool fThreadDone;
};

#define super IO80211Controller
OSDefineMetaClassAndStructors(PDBwfm, IO80211Controller)

// the pci tag is the IOPCIDevice, this finds the driver behind it
static struct {
	IOPCIDevice *pci;
	PDBwfm *owner;
} sDevices[BWFM_MAX_DEVICES];
static IOLock *sDevicesLock;

static void setOwner(IOPCIDevice *pci, PDBwfm *owner)
{
	if (sDevicesLock == NULL) {
		IOLock *lock = IOLockAlloc();

		if (!OSCompareAndSwapPtr(NULL, lock, (void * volatile *)&sDevicesLock))
			IOLockFree(lock);
	}
	IOLockLock(sDevicesLock);
	for (int i = 0; i < BWFM_MAX_DEVICES; i++) {
		if (owner != NULL && sDevices[i].pci == NULL) {
			sDevices[i].pci = pci;
			sDevices[i].owner = owner;
			break;
		}
		if (owner == NULL && sDevices[i].pci == pci) {
			sDevices[i].pci = NULL;
			sDevices[i].owner = NULL;
			break;
		}
	}
	IOLockUnlock(sDevicesLock);
}

static PDBwfm *ownerOf(void *pci)
{
	PDBwfm *owner = NULL;

	if (sDevicesLock == NULL)
		return NULL;
	IOLockLock(sDevicesLock);
	for (int i = 0; i < BWFM_MAX_DEVICES; i++) {
		if (sDevices[i].pci == pci)
			owner = sDevices[i].owner;
	}
	IOLockUnlock(sDevicesLock);
	return owner;
}

static bool rootMounted()
{
	vnode_t root = vfs_rootvnode();

	if (root == NULLVP)
		return false;
	vnode_put(root);
	return true;
}

bool PDBwfm::start(IOService *provider)
{
	struct pci_attach_args pa;
	thread_t thread;

	fThreadDone = true;
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	if (!fPCI->open(this)) {
		LOG("cannot open the pci device");
		return false;
	}
	fPollLoop = IOWorkLoop::workLoop();
	if (fPollLoop == NULL) {
		fPCI->close(this);
		return false;
	}
	setOwner(fPCI, this);
	fPCI->setMemoryEnable(true);
	fPCI->setBusMasterEnable(true);

	fDma.pci = fPCI;
	bzero(&pa, sizeof(pa));
	pa.pa_pc = fPCI;
	pa.pa_tag = fPCI;
	pa.pa_dmat = &fDma;
	pa.pa_id = fPCI->configRead32(kIOPCIConfigVendorID);
	LOG("attach: pci %04x:%04x, command 0x%04x", pa.pa_id & 0xffff, pa.pa_id >> 16,
	    fPCI->configRead16(kIOPCIConfigCommand));

	fDev = pd_bwfm_alloc(PD_BWFM_NAME);
	if (fDev == NULL || !pd_bwfm_match(&pa) || pd_bwfm_attach(fDev, &pa) != 0) {
		LOG("the pci side did not attach");
		teardown();
		return false;
	}
	fAttached = true;

	// the firmware comes off the root file system. Waiting for it here would hold up the
	// matching thread, so the rest of the bring-up has a thread of its own
	fThreadDone = false;
	retain();
	if (kernel_thread_start(&PDBwfm::firmwareThread, this, &thread) != KERN_SUCCESS) {
		release();
		fThreadDone = true;
		LOG("no thread for the firmware bring-up");
		teardown();
		return false;
	}
	thread_deallocate(thread);
	LOG("attach: pci side done, firmware bring-up waits for the root file system");
	return true;
}

void PDBwfm::firmwareThread(void *arg, wait_result_t wr)
{
	PDBwfm *self = (PDBwfm *)arg;

	self->bringUp();
	self->fThreadDone = true;
	self->release();
}

void PDBwfm::bringUp()
{
	for (int i = 0; i < BWFM_ROOT_WAIT_S * 10 && !rootMounted() && !fStopping; i++)
		IOSleep(100);
	if (fStopping)
		return;
	if (!rootMounted()) {
		LOG("no root file system after %d s, giving up", BWFM_ROOT_WAIT_S);
		return;
	}
	LOG("root file system mounted, loading firmware from %s", PD_BWFM_FIRMWARE_DIR);
	if (pd_bwfm_attach_hook(fDev) != 0) {
		LOG("firmware bring-up FAILED, no interface");
		return;
	}
	if (fStopping)
		return;
	LOG("firmware bring-up done, attaching the 802.11 interface");
	if (!attach80211(pd_bwfm_ic(fDev))) {
		LOG("attach80211 failed");
		return;
	}
	f80211 = true;
	registerService();
	LOG("802.11 interface attached");
	// There is no configd/InterfaceNamer in the RAM root: ask the stack to name every
	// published interface, which is what IORegisterNetworkInterface does from userspace.
	for (int i = 0; i < 10 && !fStopping; i++) {
		OSObject *bsd = fInterface ? ((IOService *)fInterface)->copyProperty("BSD Name") : NULL;
		OSString *name = OSDynamicCast(OSString, bsd);

		if (name != NULL) {
			LOG("interface is %s", name->getCStringNoCopy());
			OSSafeReleaseNULL(bsd);
			break;
		}
		OSSafeReleaseNULL(bsd);
		OSDictionary *stackMatch = serviceMatching("IONetworkStack");
		IOService *st = stackMatch ? copyMatchingService(stackMatch) : NULL;
		OSDictionary *cmd = OSDictionary::withCapacity(1);
		OSNumber *all = OSNumber::withNumber((unsigned long long)2 /* kIONetworkStackRegisterInterfaceAll */, 32);
		if (st && cmd && all) {
			cmd->setObject("IONetworkStackUserCommand", all);
			st->setProperties(cmd);
		}
		OSSafeReleaseNULL(all);
		OSSafeReleaseNULL(cmd);
		OSSafeReleaseNULL(st);
		OSSafeReleaseNULL(stackMatch);
		IOSleep(500);
	}
}

void PDBwfm::teardown()
{
	// no handler may run once the softc goes away
	if (fIntSrc != NULL) {
		fIntSrc->disable();
		if (fPollLoop != NULL)
			fPollLoop->removeEventSource(fIntSrc);
		OSSafeReleaseNULL(fIntSrc);
	}
	if (fPollTimer != NULL) {
		fPolling = false;
		fPollTimer->cancelTimeout();
		fPollTimer->disable();
		if (fPollLoop != NULL)
			fPollLoop->removeEventSource(fPollTimer);
		OSSafeReleaseNULL(fPollTimer);
	}
	if (fAttached)
		pd_bwfm_detach(fDev);
	fAttached = false;
	if (fDev != NULL)
		pd_bwfm_free(fDev);
	fDev = NULL;
	for (int i = 0; i < BWFM_MAX_BARS; i++)
		OSSafeReleaseNULL(fBars[i]);
	OSSafeReleaseNULL(fPollLoop);
	if (fPCI != NULL) {
		setOwner(fPCI, NULL);
		fPCI->close(this);
	}
}

void PDBwfm::stop(IOService *provider)
{
	fStopping = true;
	while (!fThreadDone)
		IOSleep(50);
	if (f80211)
		detach80211();
	f80211 = false;
	teardown();
	super::stop(provider);
}

const OSString *PDBwfm::newVendorString() const
{
	return OSString::withCString("Broadcom");
}

const OSString *PDBwfm::newModelString() const
{
	return OSString::withCString("BCM4378 802.11ax");
}

// openbsd runs the interrupt handler at IPL_NET, here it runs on the poll work loop under the stack lock
void PDBwfm::pollAction(OSObject *owner, IOTimerEventSource *sender)
{
	// the pool logs and clears DART faults; with MSI this timer is only a slow safety net
	static unsigned ticks;
	if (++ticks % 16 == 0)
		pd_dart_pool_check("poll");
	PDBwfm *self = (PDBwfm *)owner;

	if (!self->fPolling)
		return;
	pd80211_lock();
	if (self->fIntrFn != NULL)
		self->fIntrFn(self->fIntrArg);
	pd80211_unlock();
	if (self->fPolling)
		sender->setTimeoutUS(self->fPollUS);
}

// the openbsd calls bwfm makes

extern "C" {

// every file asked for is logged with its result, the names are bwfm's fallback chain
int loadfirmware(const char *name, u_char **bufp, size_t *sizep)
{
	char path[MAXPATHLEN];
	vfs_context_t ctx;
	vnode_t vp = NULLVP;
	struct vnode_attr va;
	u_char *buf = NULL;
	size_t len = 0;
	int error, resid = 0;

	snprintf(path, sizeof(path), "%s/%s", PD_BWFM_FIRMWARE_DIR, name);
	if (!rootMounted()) {
		LOG("firmware file %s: no root file system", path);
		return ENOENT;
	}
	ctx = vfs_context_create(NULL);
	error = vnode_open(path, FREAD, 0, 0, &vp, ctx);
	if (error != 0) {
		vfs_context_rele(ctx);
		LOG("firmware file %s: not found (error %d)", path, error);
		return error;
	}

	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	error = vnode_getattr(vp, &va, ctx);
	if (error == 0 && (va.va_data_size == 0 || va.va_data_size > 32 * 1024 * 1024))
		error = EFBIG;
	if (error == 0) {
		len = (size_t)va.va_data_size;
		buf = (u_char *)pd80211_malloc(len, 2, 0);
		if (buf == NULL)
			error = ENOMEM;
	}
	if (error == 0) {
		error = vn_rdwr(UIO_READ, vp, (caddr_t)buf, (int)len, 0, UIO_SYSSPACE, 0,
		    vfs_context_ucred(ctx), &resid, vfs_context_proc(ctx));
		if (error == 0 && resid != 0)
			error = EIO;
	}

	vnode_close(vp, FREAD, ctx);
	vfs_context_rele(ctx);
	if (error != 0) {
		if (buf != NULL)
			pd80211_free(buf, 2, len);
		LOG("firmware file %s: found but unreadable (error %d)", path, error);
		return error;
	}
	LOG("firmware file %s: found, %zu bytes", path, len);
	*bufp = buf;
	*sizep = len;
	return 0;
}

// a property of the Apple device tree, as XNU publishes it in the IORegistry
int pd_bwfm_adt_prop(const char *path, const char *name, void *buf, int len)
{
	IORegistryEntry *entry = IORegistryEntry::fromPath(path, gIODTPlane);
	OSData *data;
	int n = -1;

	if (entry == NULL)
		return -1;
	data = OSDynamicCast(OSData, entry->getProperty(name));
	if (data != NULL) {
		n = (int)data->getLength();
		if (buf != NULL && len > 0)
			memcpy(buf, data->getBytesNoCopy(), n < len ? n : len);
	}
	entry->release();
	return n;
}

int pd_bwfm_bootarg(const char *name, char *buf, int len)
{
	char value[64];

	if (!PE_parse_boot_argn(name, value, sizeof(value)) || value[0] == '\0')
		return 0;
	strlcpy(buf, value, len);
	return 1;
}

int pd_bwfm_want_fwsup(void)
{
	int value = 1;

	PE_parse_boot_argn("bwfm_fwsup", &value, sizeof(value));
	return value != 0;
}

// config space, the tag is the IOPCIDevice

pcireg_t pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	return ((IOPCIDevice *)tag)->configRead32(reg);
}

void pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t val)
{
	((IOPCIDevice *)tag)->configWrite32(reg, val);
}

// bwfm maps two BARs, the register window (0x10) and the chip's memory (0x18)
int pci_mapreg_map(struct pci_attach_args *pa, int reg, pcireg_t type, int flags,
    bus_space_tag_t *tagp, bus_space_handle_t *handlep, bus_addr_t *basep,
    bus_size_t *sizep, bus_size_t maxsize)
{
	IOPCIDevice *pci = (IOPCIDevice *)pa->pa_tag;
	PDBwfm *owner = ownerOf(pci);
	IOMemoryMap *map;
	int slot = -1;

	if (owner == NULL)
		return ENXIO;
	for (int i = 0; i < BWFM_MAX_BARS && slot < 0; i++) {
		if (owner->fBars[i] == NULL)
			slot = i;
	}
	if (slot < 0)
		return ENOMEM;
	// Apple PCIe windows must be mapped posted-write (Device-nGnRE): a non-posted write faults (SError)
	map = pci->mapDeviceMemoryWithRegister((UInt8)reg, kIOMapPostedWrite);
	if (map == NULL) {
		LOG("attach: BAR at config 0x%02x did not map", reg);
		return ENOMEM;
	}
	owner->fBars[slot] = map;
	LOG("attach: BAR at config 0x%02x mapped, physical 0x%llx, %llu bytes", reg,
	    (unsigned long long)map->getPhysicalAddress(), (unsigned long long)map->getLength());
	if (tagp != NULL)
		*tagp = 0;
	if (handlep != NULL)
		*handlep = (bus_space_handle_t)map->getVirtualAddress();
	if (basep != NULL)
		*basep = map->getPhysicalAddress();
	if (sizep != NULL)
		*sizep = map->getLength();
	return 0;
}

int pci_matchbyid(struct pci_attach_args *pa, const struct pci_matchid *ids, int n)
{
	for (int i = 0; i < n; i++) {
		if (PCI_VENDOR(pa->pa_id) == ids[i].pm_vid && PCI_PRODUCT(pa->pa_id) == ids[i].pm_pid)
			return 1;
	}
	return 0;
}

// "establishing" the interrupt sets up one MSI vector and a slow fallback poll (fast poll if MSI is unavailable)

int pci_intr_map_msi(struct pci_attach_args *pa, pci_intr_handle_t *ih)
{
	ih->ih_index = 0;
	return 0;
}

int pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ih)
{
	ih->ih_index = 0;
	return 0;
}

const char *pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	return "msi";
}

void *pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih, int level,
    int (*fn)(void *), void *arg, const char *name)
{
	PDBwfm *owner = ownerOf(pc);

	if (owner == NULL || owner->fPollTimer != NULL)
		return NULL;
	owner->fIntrFn = fn;
	owner->fIntrArg = arg;
	owner->fPollTimer = IOTimerEventSource::timerEventSource(owner, &PDBwfm::pollAction);
	if (owner->fPollTimer == NULL ||
	    owner->fPollLoop->addEventSource(owner->fPollTimer) != kIOReturnSuccess) {
		OSSafeReleaseNULL(owner->fPollTimer);
		return NULL;
	}
	owner->fPolling = true;
	owner->fPollUS = PD_BWFM_POLL_US;
	if (owner->setupMSI())
		owner->fPollUS = PD_BWFM_MSI_FALLBACK_POLL_US;
	owner->fPollTimer->enable();
	owner->fPollTimer->setTimeoutUS(owner->fPollUS);
	LOG("attach: %s, handler also polled every %u us", owner->fIntSrc ? "MSI interrupt" : "no interrupt", owner->fPollUS);
	return owner;
}

void PDBwfm::intrAction(OSObject *owner, IOInterruptEventSource *sender, int count)
{
	PDBwfm *self = (PDBwfm *)owner;

	if (self->fIntrCount++ == 0)
		LOG("first MSI interrupt delivered");
	if (!self->fPolling)
		return;
	pd80211_lock();
	if (self->fIntrFn != NULL)
		self->fIntrFn(self->fIntrArg);
	pd80211_unlock();
}

/*
 * One MSI vector. The port's MSI block was enabled when the link came up (doorbell
 * 0xfffff000, base 0), so message data n arrives as AIC interrupt msi-vector-offset + n.
 * The interrupt is attached to this service, not the PCI nub, whose interrupt state
 * IOPCIFamily already resolved without a controller.
 */
bool PDBwfm::setupMSI()
{
	IORegistryEntry *apcie = IORegistryEntry::fromPath("/arm-io/apcie", gIODTPlane);
	OSData *off = apcie ? OSDynamicCast(OSData, apcie->getProperty("msi-vector-offset")) : NULL;
	OSData *addr = apcie ? OSDynamicCast(OSData, apcie->getProperty("msi-address")) : NULL;
	IOInterruptVectorNumber vector = 704;
	UInt32 doorbell = 0xfffff000;
	UInt8 cap = 0;
	bool ok = false;

	if (off && off->getLength() >= 4)
		vector = *(const UInt32 *)off->getBytesNoCopy();
	if (addr && addr->getLength() >= 4)
		doorbell = *(const UInt32 *)addr->getBytesNoCopy();
	OSSafeReleaseNULL(apcie);
	if (fPCI->findPCICapability(kIOPCIMSICapability, &cap) == 0 || cap == 0) {
		LOG("msi: the device has no MSI capability");
		return false;
	}

	const OSSymbol *controller = gIODTDefaultInterruptController;
	OSData *spec = OSData::withBytes(&vector, sizeof(vector));
	OSArray *controllers = OSArray::withObjects((const OSObject **)&controller, 1);
	OSArray *specs = spec ? OSArray::withObjects((const OSObject **)&spec, 1) : NULL;
	if (controllers && specs) {
		setProperty(gIOInterruptControllersKey, controllers);
		setProperty(gIOInterruptSpecifiersKey, specs);
		fIntSrc = IOInterruptEventSource::interruptEventSource(this, &PDBwfm::intrAction, this, 0);
	}
	OSSafeReleaseNULL(spec);
	OSSafeReleaseNULL(controllers);
	OSSafeReleaseNULL(specs);
	if (fIntSrc == NULL || fPollLoop->addEventSource(fIntSrc) != kIOReturnSuccess) {
		LOG("msi: no interrupt event source for AIC vector %d", (int)vector);
		OSSafeReleaseNULL(fIntSrc);
		return false;
	}
	fIntSrc->enable();

	UInt16 ctl = fPCI->configRead16(cap + 2);
	fPCI->configWrite32(cap + 4, doorbell);
	if (ctl & 0x80) {			/* 64-bit capable */
		fPCI->configWrite32(cap + 8, 0);
		fPCI->configWrite16(cap + 12, 0);
	} else {
		fPCI->configWrite16(cap + 8, 0);
	}
	fPCI->configWrite16(cap + 2, (ctl & ~0x70) | 0x1);	/* one message, enable */
	fPCI->configWrite16(kIOPCIConfigCommand, fPCI->configRead16(kIOPCIConfigCommand) | 0x400); /* INTx off */
	ok = (fPCI->configRead16(cap + 2) & 1) != 0;
	LOG("msi: capability at 0x%x control 0x%04x -> 0x%04x, doorbell 0x%08x, AIC vector %d, %s",
	    cap, ctl, fPCI->configRead16(cap + 2), doorbell, (int)vector, ok ? "enabled" : "NOT enabled");
	return ok;
}

void pci_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
{
	PDBwfm *owner = (PDBwfm *)cookie;

	if (owner == NULL || owner->fPollTimer == NULL)
		return;
	owner->fPolling = false;
	owner->fPollTimer->cancelTimeout();
}

}
