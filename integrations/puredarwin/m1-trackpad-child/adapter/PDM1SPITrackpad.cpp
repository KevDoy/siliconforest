#include "PDM1SPITrackpad.h"
#include "PDM1SPIHID.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>

#define super IOHIDDevice
OSDefineMetaClassAndStructors(PDM1SPITrackpad,IOHIDDevice);

bool PDM1SPITrackpad::initWithSnapshot(const uint8_t *bytes, size_t length,
    uint16_t vid, uint16_t pid, uint16_t ver, uint8_t ctry)
{
    phase=0; privateStopRunning=false; privateStopDone=false; baseStarted=false;
    if (!bytes || length!=sizeof(descriptor) || !super::init()) return false;
    bcopy(bytes,descriptor,sizeof(descriptor));
    vendor=vid; product=pid; version=ver; country=ctry;
    OSNumber *iface=OSNumber::withNumber(2,32);
    bool okay=iface && setProperty(DL_M1_TP_INTERFACE_PROPERTY,iface) &&
        getProperty(DL_M1_TP_INTERFACE_PROPERTY)==iface;
    if (iface) iface->release();
    return okay && setProperty(kIOHIDRegisterServiceKey,kOSBooleanFalse) &&
        getProperty(kIOHIDRegisterServiceKey)==kOSBooleanFalse &&
        setProperty("Built-In",kOSBooleanTrue) &&
        setProperty("HIDDefaultBehavior",kOSBooleanTrue);
}
bool PDM1SPITrackpad::committed() const
{ return __atomic_load_n(&phase,__ATOMIC_ACQUIRE)!=0; }
bool PDM1SPITrackpad::admitting() const
{ return __atomic_load_n(&phase,__ATOMIC_ACQUIRE)==1; }
void PDM1SPITrackpad::markCommitted()
{ __atomic_store_n(&phase,1,__ATOMIC_RELEASE); }
void PDM1SPITrackpad::markRetired()
{
    if (committed()) {
        __atomic_store_n(&phase,2,__ATOMIC_RELEASE);
        /* Even a late stop with no provider argument must invalidate this
         * child's admission record without dynamically resolving a loop. */
        removeProperty(DL_M1_TP_CHILD_STATE_PROPERTY);
    }
}

bool PDM1SPITrackpad::start(IOService *provider)
{
    if (getProperty(kIOHIDRegisterServiceKey)!=kOSBooleanFalse) return false;
    /* Inherited getWorkLoop uses the attached provider only during base start.
     * Base IOHIDDevice retains _workLoop. No dynamic loop lookup after detach. */
    baseStarted=super::start(provider);
    IOLog("PDM1SPITrackpad: descriptor parse/interface allocation base_start=%u bytes=%u (no input proof)\n",
          baseStarted?1u:0u,(unsigned)sizeof(descriptor));
    return baseStarted;
}
void PDM1SPITrackpad::clearProviderSink(IOService *provider)
{
    PDM1SPIHID *parent=OSDynamicCast(PDM1SPIHID,provider);
    if (parent) parent->retireChild(this);
}
bool PDM1SPITrackpad::willTerminate(IOService *provider, IOOptionBits options)
{
    markRetired();
    clearProviderSink(provider);
    return super::willTerminate(provider,options);
}
void PDM1SPITrackpad::stop(IOService *provider)
{
    markRetired();
    /* M2: may already hold the inherited loop's recursive gate. This neither
     * waits for dispatches nor resolves getWorkLoop() after detach. */
    clearProviderSink(provider);
    if (committed()) {
        IOLog("PDM1SPITrackpad: TEARDOWN_DEFERRED committed cohort retained\n");
        return;
    }
    if (privateStopDone || privateStopRunning) return;
    privateStopRunning=true;
    super::stop(provider);
    privateStopRunning=false; privateStopDone=true;
}
void PDM1SPITrackpad::handleStop(IOService *provider)
{
    markRetired(); clearProviderSink(provider);
    if (committed()) return;
    super::handleStop(provider);
}
void PDM1SPITrackpad::free()
{
    if (committed()) {
        IOLog("PDM1SPITrackpad: invariant violation; committed free refused\n");
        return;
    }
    super::free();
}
bool PDM1SPITrackpad::handleOpen(IOService *client, IOOptionBits options, void *argument)
{ return admitting() && !isInactive() && super::handleOpen(client,options,argument); }
IOReturn PDM1SPITrackpad::newUserClient(task_t, void *, UInt32, OSDictionary *, IOUserClient **out)
{ if (out) *out=nullptr; return kIOReturnUnsupported; }
IOReturn PDM1SPITrackpad::newReportDescriptor(IOMemoryDescriptor **out) const
{
    if (!out) return kIOReturnBadArgument;
    *out=IOBufferMemoryDescriptor::withBytes(descriptor,sizeof(descriptor),kIODirectionNone);
    return *out?kIOReturnSuccess:kIOReturnNoMemory;
}
IOReturn PDM1SPITrackpad::getReport(IOMemoryDescriptor *, IOHIDReportType, IOOptionBits)
{ return kIOReturnUnsupported; }
IOReturn PDM1SPITrackpad::setReport(IOMemoryDescriptor *, IOHIDReportType, IOOptionBits)
{ return kIOReturnUnsupported; }
IOReturn PDM1SPITrackpad::getReport(IOMemoryDescriptor *, IOHIDReportType, IOOptionBits, UInt32, IOHIDCompletion *)
{ return kIOReturnUnsupported; }
IOReturn PDM1SPITrackpad::setReport(IOMemoryDescriptor *, IOHIDReportType, IOOptionBits, UInt32, IOHIDCompletion *)
{ return kIOReturnUnsupported; }
OSString *PDM1SPITrackpad::newTransportString() const { return OSString::withCString("SPI"); }
OSString *PDM1SPITrackpad::newProductString() const { return OSString::withCString("Apple SPI Trackpad (J293 passive)"); }
OSNumber *PDM1SPITrackpad::newVendorIDNumber() const { return OSNumber::withNumber(vendor,32); }
OSNumber *PDM1SPITrackpad::newProductIDNumber() const { return OSNumber::withNumber(product,32); }
OSNumber *PDM1SPITrackpad::newVersionNumber() const { return OSNumber::withNumber(version,32); }
OSNumber *PDM1SPITrackpad::newCountryCodeNumber() const { return OSNumber::withNumber(country,32); }
