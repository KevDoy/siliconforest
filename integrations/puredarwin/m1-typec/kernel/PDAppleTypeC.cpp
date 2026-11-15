// SPDX-License-Identifier: GPL-2.0
/*
 * J293 CD321x transport using the existing Linux PA Semi I2C port.
 * Register reads follow drivers/usb/typec/tipd/core.c (linux-aurora 6a08a08e):
 * Copyright (C) 2017, Intel Corporation
 * Author: Heikki Krogerus <heikki.krogerus@linux.intel.com>
 * Copyright (C) The Asahi Linux Contributors
 *
 * Port 1 only: perform the Linux S0/SSPS initialization when necessary,
 * then observe negotiated state. No role swap, IRQ acknowledge, firmware
 * update, mux write or port-0 transaction is sent.
 */
#include "pd_i2c.h"
#include "pd_cd321x.h"
#include "pd_cd321x_power.h"
#include <mach/mach_time.h>
#include <IOKit/IOService.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOTimerEventSource.h>
#include <libkern/OSAtomic.h>
#include <pexpert/pexpert.h>

#define LOG(fmt, ...) IOLog("PDAppleTypeC: " fmt "\n", ##__VA_ARGS__)
static const char *const hpmPath = "/arm-io/i2c0/hpmBusManager/hpm1";

class PDAppleTypeC : public IOService {
    OSDeclareDefaultStructors(PDAppleTypeC)
    PDI2C bus;
    IOWorkLoop *loop;
    IOTimerEventSource *timer;
    uint8_t address;
    uint32_t lastStatus, lastData, lastVdo, lastFlags, samples, failures;
    bool valid;
    int blockRead(uint8_t reg, void *dst, unsigned len);
    int blockWrite(uint8_t reg,const void *src,unsigned len);
    static int powerRead(void *p,uint8_t reg,void *dst,size_t len) { return ((PDAppleTypeC *)p)->blockRead(reg,dst,(unsigned)len); }
    static int powerWrite(void *p,uint8_t reg,const void *src,size_t len) { return ((PDAppleTypeC *)p)->blockWrite(reg,src,(unsigned)len); }
    static uint64_t powerNow(void *) { uint64_t ns;absolutetime_to_nanoseconds(mach_absolute_time(),&ns);return ns/1000000; }
    static void powerSleep(void *,unsigned ms) { IOSleep(ms); }
    void sample();
    static void tick(OSObject *owner, IOTimerEventSource *source);
public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;
};

#define super IOService
OSDefineMetaClassAndStructors(PDAppleTypeC, IOService)
static volatile UInt32 claimed;

IOService *PDAppleTypeC::probe(IOService *provider, SInt32 *score)
{
    int enabled = 0;
    uint32_t chip = 0;
    if (!PE_parse_boot_argn("pdtypec", &enabled, sizeof(enabled)) || enabled != 1)
        return NULL;
    if (!pd_adt_u32("/chosen", "chip-id", 0, &chip) || chip != 0x8103)
        return NULL;
    // The installed boot chain removes the bench's port-0 nodes. Require the
    // expected surviving CD3217 node/address, rather than probing other buses.
    uint32_t addr = 0;
    if (!pd_adt_u32(hpmPath, "hpm-iic-addr", 0, &addr) || addr != 0x3f)
        return NULL;
    IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
    bool board = root && IODTMatchNubWithKeys(root, "J293AP");
    OSSafeReleaseNULL(root);
    if (!board || !OSCompareAndSwap(0, 1, &claimed))
        return NULL;
    address = (uint8_t)addr;
    return super::probe(provider, score);
}

int PDAppleTypeC::blockRead(uint8_t reg, void *dst, unsigned len)
{
    uint8_t wire[PD_TPS_MAX_LEN + 1];
    if (!len || len > PD_TPS_MAX_LEN)
        return EINVAL;
    PDI2CMsg msg[2] = {{address, false, &reg, 1}, {address, true, wire, len + 1}};
    int ret = bus.transfer(msg, 2);
    return ret ? ret : pd_cd321x_unpack(wire, len + 1, dst, len);
}

int PDAppleTypeC::blockWrite(uint8_t reg,const void *src,unsigned len)
{
    if(!src || !len || len>PD_TPS_MAX_LEN)return EINVAL;
    uint8_t wire[PD_TPS_MAX_LEN+2];wire[0]=reg;wire[1]=(uint8_t)len;
    memcpy(wire+2,src,len);
    PDI2CMsg msg={address,false,wire,len+2};return bus.transfer(&msg,1);
}

void PDAppleTypeC::sample()
{
    uint8_t status[4], data[4], intel[9];
    int ret = blockRead(PD_TPS_REG_STATUS, status, sizeof(status));
    if (!ret)
        ret = blockRead(PD_TPS_REG_DATA_STATUS, data, sizeof(data));
    uint32_t vdo=0, flags=0;
    if (!ret && (pd_cd321x_le32(data) & PD_TPS_DATA_TBT)) {
        ret = blockRead(PD_TPS_REG_INTEL_VID_STATUS, intel, sizeof(intel));
        if (!ret) { vdo=pd_cd321x_tbt_vdo(intel); flags=PD_CD321X_TBT_VALID; }
    }
    samples++;
    setProperty("sample-count", samples, 32);
    setProperty("last-error", (uint32_t)ret, 32);
    setProperty("sample-valid", ret == 0);
    if (ret) {
        failures++;
        setProperty("error-count", failures, 32);
        if (failures == 1 || failures % 60 == 0)
            LOG("port 1 status read failed errno=%d (failed samples=%u)", ret, failures);
        struct pd_cd321x_cable_snapshot snapshot = {{lastStatus, lastData, samples, (uint32_t)ret},lastVdo,lastFlags};
        setProperty("port-state", &snapshot, sizeof(snapshot));
        messageClients(kIOMessageServicePropertyChange);
        // Failed samples never become disconnects or usable role changes.
        return;
    }
    uint32_t s = pd_cd321x_le32(status), d = pd_cd321x_le32(data);
    setProperty("status", s, 32);
    setProperty("data-status", d, 32);
    setProperty("plug-present", (bool)(s & PD_TPS_STATUS_PLUG_PRESENT));
    setProperty("orientation-reversed", (bool)(s & PD_TPS_STATUS_UPSIDE_DOWN));
    setProperty("pd-data-role-host", (bool)(s & PD_TPS_STATUS_DATAROLE));
    enum pd_cd321x_usb_role role = pd_cd321x_usb_role(s, d);
    setProperty("usb-role", role == PD_CD321X_USB_HOST ? "host" :
        role == PD_CD321X_USB_DEVICE ? "device" : "none");
    setProperty("usb2", (bool)(d & PD_TPS_DATA_USB2));
    setProperty("usb3", (bool)(d & PD_TPS_DATA_USB3));
    setProperty("displayport", (bool)(d & PD_TPS_DATA_DP));
    setProperty("thunderbolt", (bool)(d & PD_TPS_DATA_TBT));
    setProperty("usb4", (bool)(d & PD_CD321X_DATA_USB4));
    setProperty("hpd", (bool)(d & PD_CD321X_DATA_HPD));
    if (!valid || s != lastStatus || d != lastData)
        LOG("port 1 status=%08x data=%08x present=%u reverse=%u pd_host=%u usb2=%u usb3=%u dp=%u tbt=%u usb4=%u hpd=%u (observed, PHY/host not started)",
            s, d, !!(s & PD_TPS_STATUS_PLUG_PRESENT), !!(s & PD_TPS_STATUS_UPSIDE_DOWN),
            !!(s & PD_TPS_STATUS_DATAROLE), !!(d & PD_TPS_DATA_USB2), !!(d & PD_TPS_DATA_USB3),
            !!(d & PD_TPS_DATA_DP), !!(d & PD_TPS_DATA_TBT), !!(d & PD_CD321X_DATA_USB4),
            !!(d & PD_CD321X_DATA_HPD));
    lastStatus = s;
    lastData = d;
    lastVdo = vdo;
    lastFlags = flags;
    valid = true;
    struct pd_cd321x_cable_snapshot snapshot = {{s, d, samples, 0},vdo,flags};
    setProperty("port-state", &snapshot, sizeof(snapshot));
    messageClients(kIOMessageServicePropertyChange);
}

void PDAppleTypeC::tick(OSObject *owner, IOTimerEventSource *source)
{
    PDAppleTypeC *self = OSDynamicCast(PDAppleTypeC, owner);
    if (!self) return;
    self->sample();
    source->setTimeoutMS(1000);
}

bool PDAppleTypeC::start(IOService *provider)
{
    if (!super::start(provider)) return false;
    setProperty("port-index", 1, 32);
    setProperty("port-label", "USB-C Left-front");
    setProperty("observer-only", false);
    setProperty("setup-policy","port1-S0-only");
    setProperty("hpm-path", hpmPath);
    setProperty("error-count", (uint32_t)0, 32);
    if (!bus.attach("/arm-io/i2c0", 100000)) {
        LOG("I2C0 attach failed");
        return false;
    }
    uint8_t vid[4], mode[4];
    int ret = blockRead(PD_TPS_REG_VID, vid, sizeof(vid));
    if (!ret) ret = blockRead(PD_TPS_REG_MODE, mode, sizeof(mode));
    if (ret || !pd_cd321x_le32(vid)) {
        LOG("CD321x identity/mode read failed errno=%d", ret ? ret : ENODEV);
        return false;
    }
    setProperty("vendor-id", pd_cd321x_le32(vid), 32);
    setProperty("firmware-mode", mode, sizeof(mode));
    if(memcmp(mode,"APP ",4)) { LOG("port1 not APP; no SSPS or PHY admission");return false; }
    pd_cd321x_power_io io={this,powerRead,powerWrite,powerNow,powerSleep};
    pd_cd321x_power_result power={};ret=pd_cd321x_ensure_s0(&io,&power);
    setProperty("system-power-before",(uint32_t)power.before,32);
    setProperty("system-power-after",(uint32_t)power.after,32);
    setProperty("system-power-command-issued",power.command_issued);
    setProperty("system-power-polls",(uint32_t)power.polls,32);
    setProperty("system-power-error",(uint32_t)ret,32);
    LOG("port1 S0 before=%u after=%u SSPS=%u polls=%u error=%d; port0 reserved",
        power.before,power.after,power.command_issued,power.polls,ret);
    if(ret)return false; // No state publication, PHY or ACIO start after failed setup.
    loop = IOWorkLoop::workLoop();
    timer = IOTimerEventSource::timerEventSource(this, tick);
    if (!loop || !timer || loop->addEventSource(timer) != kIOReturnSuccess)
        return false;
    timer->setTimeoutMS(1);
    registerService();
    return true;
}

void PDAppleTypeC::stop(IOService *provider)
{
    if (timer) {
        timer->cancelTimeout();
        timer->disable();
        // removeEventSource serializes with the workloop's current callback.
        if (loop) loop->removeEventSource(timer);
    }
    super::stop(provider);
}

void PDAppleTypeC::free()
{
    OSSafeReleaseNULL(timer);
    OSSafeReleaseNULL(loop);
    bus.regs.unmap();
    if (bus.lock) { IOLockFree(bus.lock); bus.lock = NULL; }
    super::free();
}
