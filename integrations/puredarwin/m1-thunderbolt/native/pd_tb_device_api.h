/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_DEVICE_API_H
#define PD_TB_DEVICE_API_H
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include "pd_tb_native_abi.h"
/* Driver-model entry points required by the upstream CM. Declarations do
 * not implement native registration, PM, uevents or hardware completion. */
struct bus_type {
 const char *name;
 int (*match)(struct device *,const struct device_driver *);
 int (*probe)(struct device *);
 void (*remove)(struct device *);
 void (*shutdown)(struct device *);
};
int bus_register(const struct bus_type *);
void bus_unregister(const struct bus_type *);
int driver_register(struct device_driver *);
void driver_unregister(struct device_driver *);
int bus_for_each_dev(const struct bus_type *,struct device *,void *,int (*)(struct device *,void *));
struct device *bus_find_device(const struct bus_type *,struct device *,const void *,int (*)(struct device *,const void *));
int device_for_each_child(struct device *,void *,int (*)(struct device *,void *));
int device_for_each_child_reverse(struct device *,void *,int (*)(struct device *,void *));
struct device *device_find_child(struct device *,const void *,int (*)(struct device *,const void *));
int add_uevent_var(struct kobj_uevent_env *,const char *,...) __attribute__((format(printf,2,3)));
int kobject_uevent(struct kobject *,int);
void dev_set_uevent_suppress(struct device *,bool);
struct kobj_uevent_env { const char *envp[32]; size_t count,used; char buf[2048]; };
void pd_tb_device_initialize(struct device *);
int pd_tb_device_add(struct device *);
void pd_tb_device_del(struct device *);
int pd_tb_device_register(struct device *);
void pd_tb_device_unregister(struct device *);
int pd_tb_runtime(struct device *,enum pd_tb_pm_op,int);
void pd_tb_native_fault(int error);
int pd_tb_device_init_wakeup(struct device *,bool);
bool pd_tb_device_may_wakeup(struct device *);
void pd_tb_device_set_wakeup_capable(struct device *,bool);
#define device_initialize pd_tb_device_initialize
#define device_add pd_tb_device_add
#define device_del pd_tb_device_del
#define device_register pd_tb_device_register
#define device_unregister pd_tb_device_unregister
#define device_init_wakeup pd_tb_device_init_wakeup
#define device_may_wakeup pd_tb_device_may_wakeup
#define device_set_wakeup_capable pd_tb_device_set_wakeup_capable
#undef KOBJ_CHANGE
#define KOBJ_ADD 0
#define KOBJ_REMOVE 1
#define KOBJ_CHANGE 2
/* Bypass LinuxKPI success stubs: every CM power call reaches the native owner. */
#define pm_runtime_allow(d) pd_tb_runtime((d),PD_TB_PM_ALLOW,0)
#define pm_runtime_disable(d) pd_tb_runtime((d),PD_TB_PM_DISABLE,0)
#define pm_runtime_enable(d) pd_tb_runtime((d),PD_TB_PM_ENABLE,0)
#define pm_runtime_get(d) pd_tb_runtime((d),PD_TB_PM_GET,0)
#define pm_runtime_get_noresume(d) pd_tb_runtime((d),PD_TB_PM_GET_NORESUME,0)
#define pm_runtime_get_sync(d) pd_tb_runtime((d),PD_TB_PM_GET_SYNC,0)
#define pm_runtime_mark_last_busy(d) pd_tb_runtime((d),PD_TB_PM_LAST_BUSY,0)
#define pm_runtime_no_callbacks(d) pd_tb_runtime((d),PD_TB_PM_NO_CALLBACKS,0)
#define pm_runtime_put(d) pd_tb_runtime((d),PD_TB_PM_PUT,0)
#define pm_runtime_put_autosuspend(d) pd_tb_runtime((d),PD_TB_PM_PUT_AUTOSUSPEND,0)
#define pm_runtime_put_noidle(d) pd_tb_runtime((d),PD_TB_PM_PUT_NOIDLE,0)
#define pm_runtime_resume(d) pd_tb_runtime((d),PD_TB_PM_RESUME,0)
#define pm_runtime_resume_and_get(d) pd_tb_runtime((d),PD_TB_PM_RESUME_AND_GET,0)
#define pm_runtime_set_active(d) pd_tb_runtime((d),PD_TB_PM_SET_ACTIVE,0)
#define pm_runtime_set_suspended(d) pd_tb_runtime((d),PD_TB_PM_SET_SUSPENDED,0)
#define pm_runtime_use_autosuspend(d) pd_tb_runtime((d),PD_TB_PM_USE_AUTOSUSPEND,0)
#define pm_runtime_set_autosuspend_delay(d,a) pd_tb_runtime((d),PD_TB_PM_AUTOSUSPEND_DELAY,(a))
#define pm_request_autosuspend(d) pd_tb_runtime((d),PD_TB_PM_REQUEST_AUTOSUSPEND,0)
#define pm_runtime_suspended(d) (pd_tb_runtime((d),PD_TB_PM_IS_SUSPENDED,0)==1)
#ifndef EREMOTEIO
#define EREMOTEIO EIO
#endif
#ifndef EKEYREJECTED
#define EKEYREJECTED 129 /* Linux asm-generic/errno.h */
#endif
#ifndef restart_syscall
#define restart_syscall() (-EINTR)
#endif
DEFINE_LOCK_GUARD_1(spinlock_irq,spinlock_t,
 spin_lock_irq(_T->lock),spin_unlock_irq(_T->lock))
#endif
