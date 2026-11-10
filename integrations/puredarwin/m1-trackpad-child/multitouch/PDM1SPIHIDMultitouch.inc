// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PDM1SPIHID multitouch mode: request touch data from the SPI trackpad the way
 * Omacom linux-aurora does (integration/aurora 6a08a08e), decode it
 * (m1tp_mt.c) and deliver synthesized pointer reports through the child HID
 * device. Included by PDM1SPIHID.cpp; everything runs on the transport
 * workloop.
 *
 * Mode request ported from magicmouse_enable_multitouch() in
 * drivers/hid/hid-magicmouse.c
 *   Copyright (c) 2010 Michael Poole <mdpoole@troilus.org>
 *   Copyright (c) 2010 Chase Douglas <chase.douglas@canonical.com>
 * and apple_ll_raw_request() in drivers/hid/spi-hid/spi-hid-apple-core.c
 *   Copyright (C) The Asahi Linux Contributors
 *   Copyright (c) 2015-2018 Federico Lorenzi
 *   Copyright (c) 2017-2018 Ronald Tschalär
 */

/* Linux re-sends the request whenever the input device is opened and after
 * every reset report; here a bound keeps a misbehaving controller from being
 * written to for ever. */
#define PD_M1_MT_MAX_MODE_REQUESTS 8u

/* Boot-args (all optional; the trackpad child needs pdm1trackpadchild=1 as before):
 *   pdm1tpmt=0        basic pointer mode, the behaviour before this code
 *   pdm1tpscale=N     contact units per pointer count (default 4; larger is slower)
 *   pdm1tpscroll=N    contact units of two-finger travel per wheel step (default 300)
 *   pdm1tpnatural=1   wheel direction follows the fingers
 *   pdm1tpheader=1    take pointer motion from the touch report's own pointer header
 *   pdm1tpdebug=N     print the first N decoded reports (MT_DBG lines)
 *   pdm1tpselftest=1  decoder self-test at start (MT_SELFTEST line)
 *   pdm1tpselftest=2  also deliver the self-test's pointer reports through the
 *                     child HID device when /dev/usb_hid_kbd is first opened
 *                     (MT_SELFTEST_INJECT line); for mt-selftest-prelude.in only */
void PDM1SPIHID::mtConfigure()
{
    int on=1, value=0;
    /* pdm1tpmt=0: leave the controller in its basic pointer mode, exactly as
     * before this code existed. Default: multitouch whenever the child is on. */
    if (PE_parse_boot_argn("pdm1tpmt",&on,sizeof(on)) && on!=1) on=0;
    mtEnabled=t2Enabled && on==1;
    m1tp_mt_default_config(&mtConfig);
    if (PE_parse_boot_argn("pdm1tpscale",&value,sizeof(value)) && value>0)
        mtConfig.units_per_count=(uint32_t)value;
    if (PE_parse_boot_argn("pdm1tpscroll",&value,sizeof(value)) && value>0)
        mtConfig.scroll_units_per_step=(uint32_t)value;
    if (PE_parse_boot_argn("pdm1tpnatural",&value,sizeof(value)) && value==1)
        mtConfig.natural_scroll=1;
    if (PE_parse_boot_argn("pdm1tpheader",&value,sizeof(value)) && value==1)
        mtConfig.use_header_motion=1;
    if (PE_parse_boot_argn("pdm1tpdebug",&value,sizeof(value)) && value>0)
        mtDebug=(uint32_t)(value>1000?1000:value);
    mtDebugHex=mtDebug<4?mtDebug:4;
    if (PE_parse_boot_argn("pdm1tpselftest",&value,sizeof(value)) && (value==1 || value==2))
        mtSelftest=value;
    m1tp_mt_init(&mtState,&mtConfig);
    IOLog("PDM1SPIHID: MT stage=config enabled=%u units_per_count=%u scroll_units_per_step=%u natural=%u header_motion=%u debug_reports=%u selftest=%d\n",
          mtEnabled?1u:0u,(unsigned)mtState.cfg.units_per_count,(unsigned)mtState.cfg.scroll_units_per_step,
          (unsigned)mtState.cfg.natural_scroll,(unsigned)mtState.cfg.use_header_motion,(unsigned)mtDebug,mtSelftest);
}

void PDM1SPIHID::mtSendEnable()
{
    mtModeDueUS=0;
    if (mtModeAttempts>=PD_M1_MT_MAX_MODE_REQUESTS) return;
    /* spihid_apple_request(spihid, 2, 0x52, 0x02, 0x00, 2, {0x02, 0x01}, 2):
     * SET_REPORT of feature report 2 on the trackpad interface. */
    uint8_t id=nextID++;
    if (dlsp_request(2,M1TP_MT_ENABLE_COMMAND,M1TP_MT_ENABLE_REPORT,0x00,id,
                     M1TP_MT_ENABLE_RESPONSE_BYTES,m1tp_mt_enable_payload,
                     sizeof(m1tp_mt_enable_payload),packet)!=0) {
        IOLog("PDM1SPIHID: MT stage=mode-write not encoded; staying in basic mode\n");
        return;
    }
    int e=hardware.writePacket(packet);
    mtModeAttempts++;
    IOLog("PDM1SPIHID: MT stage=mode-write attempt=%u request_id=%u result=%d\n",
          (unsigned)mtModeAttempts,(unsigned)id,e);
    /* A failed write has already faulted the shared transport. */
    if (e) fail(e,"trackpad-mode-write");
}

bool PDM1SPIHID::mtSubmit(const struct m1tp_mt_out *out)
{
    uint8_t bytes[M1TP_MT_POINTER_REPORT_BYTES];
    PDM1SPITrackpad *child=childSink;
    if (!child) return false;
    if (!child->admitting() || child->isInactive()) { retireChild(child); return false; }
    m1tp_mt_encode(out,bytes);
    IOBufferMemoryDescriptor *report=IOBufferMemoryDescriptor::withBytes(bytes,sizeof(bytes),kIODirectionNone);
    if (!report) { childFault((int32_t)kIOReturnNoMemory); return false; }
    retain(); child->retain(); t2Dispatches++;
    /* Same synchronous delivery as the basic-mode route (routeTrackpad). */
    IOReturn kr=child->handleReport(report,kIOHIDReportTypeInput);
    t2Dispatches--;
    bool okay=kr==kIOReturnSuccess;
    if (okay) {
        childSubmitted++; mtPointerReports++;
        if (mtPointerReports==1)
            IOLog("PDM1SPIHID: MT stage=first-pointer-report buttons=%02x dx=%d dy=%d wheel=%d accepted by the child HID device\n",
                  (unsigned)out->buttons,(int)out->dx,(int)out->dy,(int)out->wheel);
    } else childFault((int32_t)kr);
    report->release(); child->release(); release();
    return okay;
}

void PDM1SPIHID::mtRoute(const struct dlsp_message *message)
{
    struct m1tp_mt_out out;
    childObserved++; countersDirty=true;
    int kind=m1tp_mt_parse(message->payload,message->payload_length,&mtFrame);
    if (kind==M1TP_MT_KIND_INVALID) {
        /* Vendor reports 0x3f/0x44 and anything malformed: not pointer data. */
        childRejected++;
        if (childRejected==1)
            IOLog("PDM1SPIHID: MT stage=report-ignored id=%02x bytes=%u (first; counted in PDM1TrackpadRejected)\n",
                  message->payload_length?(unsigned)message->payload[0]:0u,(unsigned)message->payload_length);
        return;
    }
    if (kind==M1TP_MT_KIND_RESET) {
        /* magicmouse_raw_event_spi(): "Touch controller was reset,
         * re-enabling touch mode", 10 ms later. */
        mtResets++;
        if (mtModeAttempts<PD_M1_MT_MAX_MODE_REQUESTS) mtModeDueUS=PDM1KeyboardHW::clockUS()+10000;
        if (mtResets<=4)
            IOLog("PDM1SPIHID: MT stage=reset-report count=%u mode_requests=%u (requesting touch data again)\n",
                  (unsigned)mtResets,(unsigned)mtModeAttempts);
        return;
    }
    if (kind==M1TP_MT_KIND_TOUCH) {
        if (!mtTouchReports++)
            IOLog("PDM1SPIHID: MT stage=first-touch-report bytes=%u records=%u contacts=%u (controller is in multitouch mode)\n",
                  (unsigned)message->payload_length,(unsigned)mtFrame.reported,(unsigned)mtFrame.contacts);
    } else if (!mtBasicReports++ && mtModeAttempts) {
        IOLog("PDM1SPIHID: MT stage=basic-report-after-mode-write bytes=%u (passed through as before)\n",
              (unsigned)message->payload_length);
    }
    int emit=m1tp_mt_step(&mtState,&mtFrame,&out);
    uint32_t fresh=mtState.events & ~mtLoggedEvents;
    if (fresh) {
        mtLoggedEvents|=fresh;
        if (fresh & M1TP_MT_EV_HEADER_MOTION)
            IOLog("PDM1SPIHID: MT stage=pointer-header-motion dx=%d dy=%d in a touch report (used for the pointer: %u; pdm1tpheader=1 selects it)\n",
                  (int)mtFrame.mouse_dx,(int)mtFrame.mouse_dy,(unsigned)mtState.cfg.use_header_motion);
        if (fresh & M1TP_MT_EV_RIGHT_CLICK)
            IOLog("PDM1SPIHID: MT stage=first-two-finger-click reported as the right button\n");
        if (fresh & M1TP_MT_EV_SCROLL)
            IOLog("PDM1SPIHID: MT stage=first-two-finger-scroll wheel=%d\n",(int)out.wheel);
    }
    if (mtDebug) mtDump(message,emit,&out);
    if (emit) (void)mtSubmit(&out);
}

/* pdm1tpdebug=N: the first N reports that carry a contact, a button or a
 * result, decoded; the first four of them also as bytes. */
void PDM1SPIHID::mtDump(const struct dlsp_message *message, int emit, const struct m1tp_mt_out *out)
{
    static const char digits[]="0123456789abcdef";
    const struct m1tp_mt_frame *f=&mtFrame;
    if (!emit && !f->contacts && !f->reported && !f->tp_buttons && !f->mouse_buttons &&
        !f->mouse_dx && !f->mouse_dy && mtTouchReports+mtBasicReports>2) return;
    mtDebug--;
    IOLog("PDM1SPIHID: MT_DBG kind=%u bytes=%u header=%02x/%d/%d tp_buttons=%02x records=%u contacts=%u gesture=%u emit=%d out=%02x/%d/%d/%d\n",
          (unsigned)f->kind,(unsigned)message->payload_length,(unsigned)f->mouse_buttons,(int)f->mouse_dx,
          (int)f->mouse_dy,(unsigned)f->tp_buttons,(unsigned)f->reported,(unsigned)f->contacts,
          (unsigned)mtState.gesture,emit,(unsigned)out->buttons,(int)out->dx,(int)out->dy,(int)out->wheel);
    for (unsigned i=0;i<f->contacts && i<4;i++)
        IOLog("PDM1SPIHID: MT_DBG contact=%u x=%d y=%d rel=%d/%d major=%d minor=%d pressure=%d\n",
              i,(int)f->c[i].x,(int)f->c[i].y,(int)f->c[i].rel_x,(int)f->c[i].rel_y,
              (int)f->c[i].touch_major,(int)f->c[i].touch_minor,(int)f->c[i].pressure);
    if (!mtDebugHex) return;
    mtDebugHex--;
    size_t total=message->payload_length<144?message->payload_length:144;
    for (size_t offset=0;offset<total;offset+=48) {
        char hex[97];
        size_t n=total-offset<48?total-offset:48;
        for (size_t i=0;i<n;i++) {
            hex[2*i]=digits[message->payload[offset+i]>>4];
            hex[2*i+1]=digits[message->payload[offset+i]&15];
        }
        hex[2*n]=0;
        IOLog("PDM1SPIHID: MT_DBG hex offset=%u data=%s\n",(unsigned)offset,hex);
    }
}

void PDM1SPIHID::mtInjectSink(void *context, const struct m1tp_mt_out *out)
{
    PDM1SPIHID *self=static_cast<PDM1SPIHID *>(context);
    if (self->mtSubmit(out)) self->mtInjectSubmitted++;
}

/* pdm1tpselftest: constructed multitouch reports through the decoder on a
 * scratch state, every emitted pointer report compared (m1tp_mt_selftest.c).
 * With inject, the same reports are then delivered through the child HID
 * device, so they reach /dev/usb_hid_mouse like a finger's would. */
void PDM1SPIHID::mtRunSelftest(bool inject)
{
    unsigned steps=0, reports=0;
    mtInjectSubmitted=0;
    unsigned failed=m1tp_mt_selftest(&mtScratch,mtBuffer,sizeof(mtBuffer),&steps,&reports,
                                     inject?mtInjectSink:nullptr,this);
    if (!inject)
        IOLog("PDM1SPIHID: MT_SELFTEST %s steps=%u pointer_reports=%u failed_step=%u (synthetic reports, decoder only)\n",
              failed?"FAIL":"PASS",steps,reports,failed);
    else
        IOLog("PDM1SPIHID: MT_SELFTEST_INJECT %s pointer_reports=%u submitted=%u (synthetic reports through the child HID device)\n",
              !failed && reports && mtInjectSubmitted==reports?"PASS":"FAIL",reports,(unsigned)mtInjectSubmitted);
}
