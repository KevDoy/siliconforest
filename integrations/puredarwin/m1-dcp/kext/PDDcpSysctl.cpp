// SPDX-License-Identifier: GPL-2.0-only OR MIT
// PDDcpSysctl.cpp - kern.pd_backlight.*, the panel backlight's class attributes. Linux has them
// as files in /sys/class/backlight/apple-panel-bl; names, units and rules are the same:
//   kern.pd_backlight.name               string, "apple-panel-bl"
//   kern.pd_backlight.max_brightness     read:  the highest level (nits on this panel)
//   kern.pd_backlight.brightness         read:  the level last asked for
//                                      write: 0..max_brightness (EINVAL above), applied now
//   kern.pd_backlight.actual_brightness  read:  the level the display coprocessor reports
//   kern.pd_backlight.bl_power           read/write: 0 on, 4 off (the level is kept)
// ENODEV until the driver has registered the backlight (after the first mode is known).
// Under kern because that is a parent the kernel exports to kexts; hw is not exported, and the
// collection builder bound sysctl__hw_children to garbage without complaint (boot bl01).
// Writing needs root, as the sysfs files do. The work is in pd_dcp_backlight.c.

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

extern "C" {
enum { PD_DCP_BL_BRIGHTNESS, PD_DCP_BL_MAX, PD_DCP_BL_ACTUAL, PD_DCP_BL_POWER };
int pd_dcp_backlight_get(int what, int *value);
int pd_dcp_backlight_set(int what, int value);
const char *pd_dcp_backlight_name(void);
void pd_dcp_sysctl_register(void);
uint64_t pd_dcp_hotplug_seq(void);
void pd_dcp_hotplug_connectors(char *, size_t);
}

static int
bl_sysctl_int(__unused struct sysctl_oid *oidp, __unused void *arg1, int arg2, struct sysctl_req *req)
{
	int value = 0, error;

	error = pd_dcp_backlight_get(arg2, &value);
	if (error != 0)
		return error;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error != 0 || req->newptr == USER_ADDR_NULL)
		return error;
	if (arg2 != PD_DCP_BL_BRIGHTNESS && arg2 != PD_DCP_BL_POWER)
		return EPERM;
	return pd_dcp_backlight_set(arg2, value);
}

static int
bl_sysctl_name(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	const char *name = pd_dcp_backlight_name();

	if (name == NULL)
		return ENODEV;
	return SYSCTL_OUT(req, name, strlen(name) + 1);
}

SYSCTL_DECL(_kern);
SYSCTL_NODE(_kern, OID_AUTO, pd_backlight, CTLFLAG_RD | CTLFLAG_LOCKED, 0, "panel backlight (apple-panel-bl)");
SYSCTL_PROC(_kern_pd_backlight, OID_AUTO, name, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, bl_sysctl_name, "A", "backlight device name");
SYSCTL_PROC(_kern_pd_backlight, OID_AUTO, max_brightness, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, PD_DCP_BL_MAX, bl_sysctl_int, "I", "highest brightness level");
SYSCTL_PROC(_kern_pd_backlight, OID_AUTO, brightness, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, PD_DCP_BL_BRIGHTNESS, bl_sysctl_int, "I", "requested brightness level");
SYSCTL_PROC(_kern_pd_backlight, OID_AUTO, actual_brightness, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, PD_DCP_BL_ACTUAL, bl_sysctl_int, "I", "brightness level the display reports");
SYSCTL_PROC(_kern_pd_backlight, OID_AUTO, bl_power, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, PD_DCP_BL_POWER, bl_sysctl_int, "I", "0 on, 4 off");

static int
hotplug_sequence(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
    if (req->newptr != USER_ADDR_NULL) return EPERM;
    uint64_t value=pd_dcp_hotplug_seq();
    return SYSCTL_OUT(req,&value,sizeof(value));
}
static int
hotplug_connectors(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
    if (req->newptr != USER_ADDR_NULL) return EPERM;
    char text[1024];pd_dcp_hotplug_connectors(text,sizeof(text));
    return SYSCTL_OUT(req,text,strlen(text)+1);
}
SYSCTL_NODE(_kern, OID_AUTO, pd_dcp, CTLFLAG_RD | CTLFLAG_LOCKED, 0, "DCP cached connector changes");
SYSCTL_PROC(_kern_pd_dcp, OID_AUTO, hotplug_seq, CTLTYPE_QUAD | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, hotplug_sequence, "Q", "generation after cached connector status/mode updates");
SYSCTL_PROC(_kern_pd_dcp, OID_AUTO, connectors, CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, hotplug_connectors, "A", "cached connector status and preferred mode");

void
pd_dcp_sysctl_register(void)
{
	static bool done;

	if (done)
		return;
	done = true;
	sysctl_register_oid(&sysctl__kern_pd_dcp);
	sysctl_register_oid(&sysctl__kern_pd_dcp_hotplug_seq);
	sysctl_register_oid(&sysctl__kern_pd_dcp_connectors);
	sysctl_register_oid(&sysctl__kern_pd_backlight);
	sysctl_register_oid(&sysctl__kern_pd_backlight_name);
	sysctl_register_oid(&sysctl__kern_pd_backlight_max_brightness);
	sysctl_register_oid(&sysctl__kern_pd_backlight_brightness);
	sysctl_register_oid(&sysctl__kern_pd_backlight_actual_brightness);
	sysctl_register_oid(&sysctl__kern_pd_backlight_bl_power);
}
