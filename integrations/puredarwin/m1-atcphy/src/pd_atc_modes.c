// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/* T8103 configuration table from Omacom Linux atc.c (6a08a08e).
 * Copyright (C) The Asahi Linux Contributors
 * Author: Sven Peter <sven@kernel.org>
 */
#include "pd_atc_modes.h"
#include "pd_atc_dp.h"
#include <errno.h>
#include <string.h>
int pd_atc_mode_plan(enum pd_atc_mode mode,bool reverse,struct pd_atc_program *out)
{
    if(!out)return EINVAL;
    struct pd_atc_program p={0};unsigned lane0,lane1;
    switch(mode) {
    case PD_ATC_MODE_USB2:
        lane0=lane1=3;p.crossbar=reverse?0xb:0xa;p.pipe=PD_ATC_DUMMY;break;
    case PD_ATC_MODE_USB3:case PD_ATC_MODE_USB3_DP:
        lane0=reverse?2:1;lane1=reverse?1:2;
        p.crossbar=(reverse?0x11:0x10)|(8u<<5);
        p.dp_lanes=reverse?1:2;p.usb3_lanes=reverse?2:1;
        p.swap=reverse;p.aux=mode==PD_ATC_MODE_USB3_DP;p.pipe=PD_ATC_USB3_BACKEND;break;
    case PD_ATC_MODE_TBT:case PD_ATC_MODE_USB4:
        lane0=lane1=0;p.crossbar=reverse?1:0;p.usb4_lanes=3;
        // TBT3 dock USB is PCIe-attached; no DWC3 USB3 tunnel is requested.
        p.pipe=mode==PD_ATC_MODE_TBT?PD_ATC_DUMMY:PD_ATC_USB4_BACKEND;break;
    case PD_ATC_MODE_DP:
        lane0=lane1=2;p.dp_lanes=3;p.aux=true;p.pipe=PD_ATC_DUMMY;
        p.crossbar=0x14|((reverse?8u:0x100u)<<5)|(reverse?0:(1u<<17));break;
    default:return EINVAL;
    }
    p.reversed=reverse;
    p.lanes=lane0|(lane0<<3)|(lane1<<6)|(lane1<<9);
    *out=p;return 0;
}
static int aux(const struct pd_atc_io *io,unsigned mask)
{
    struct pd_atc_dp dp={.io=io,.lane_mask=mask,.rate=-1};
    // Native mode transition owns the powered/calibrated PHY here; no client
    // can enter until the remaining routing/reset steps complete.
    int ret=pd_atc_dp_aux_start(&dp);
    return ret<0?-ret:ret;
}
int pd_atc_mode_start(const struct pd_atc_io *io,enum pd_atc_mode mode,bool reversed,
                       const struct pd_atc_calibration *cal)
{
    struct pd_atc_program p;
    int ret=pd_atc_mode_plan(mode,reversed,&p);
    if(ret)return ret;
    if(mode!=PD_ATC_MODE_USB2) {
        if(!cal || !cal->validate || !cal->apply)return EINVAL;
        if((ret=cal->validate(cal->ctx,&p)))return ret;
    }
    return pd_atc_program_start(io,&p,mode==PD_ATC_MODE_USB2?NULL:cal,aux);
}
