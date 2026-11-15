/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_NATIVE_ABI_H
#define PD_TB_NATIVE_ABI_H
#include <stddef.h>
#include <stdbool.h>
/* Installed once by the serialized native controller owner, before domain
 * initialization. Publication is metadata, never a DMA/drain/power ACK.
 * Callbacks must stay valid until reboot; no replacement/unload is supported.
 * The owner serializes structural add/del/bus operations. Traversals may run
 * concurrently. lock/unlock protect only the registry, never callbacks. */
enum pd_tb_pm_op {
 PD_TB_PM_ALLOW,PD_TB_PM_DISABLE,PD_TB_PM_ENABLE,PD_TB_PM_GET,
 PD_TB_PM_GET_NORESUME,PD_TB_PM_GET_SYNC,PD_TB_PM_LAST_BUSY,
 PD_TB_PM_NO_CALLBACKS,PD_TB_PM_PUT,PD_TB_PM_PUT_AUTOSUSPEND,
 PD_TB_PM_PUT_NOIDLE,PD_TB_PM_RESUME,PD_TB_PM_RESUME_AND_GET,
 PD_TB_PM_SET_ACTIVE,PD_TB_PM_AUTOSUSPEND_DELAY,PD_TB_PM_SET_SUSPENDED,
 PD_TB_PM_IS_SUSPENDED,PD_TB_PM_USE_AUTOSUSPEND,PD_TB_PM_REQUEST_AUTOSUSPEND,PD_TB_PM_WAKE_CAPABILITY,PD_TB_PM_WAKE_POLICY
};
struct pd_tb_native_ops {
 void *ctx;
 void (*lock)(void *); void (*unlock)(void *);
 int (*publish)(void *,void *device,void *parent,const char *name,const char *type,void **token);
 void (*unpublish)(void *,void *token);
 int (*event)(void *,void *token,int action,const char *const *env,size_t count);
 /* Actual native lease/state transition; negative on unsupported/failure.
  * IS_SUSPENDED returns exactly 0 or 1. Errors latch the adapter closed. */
 int (*runtime)(void *,void *device,enum pd_tb_pm_op,int argument);
 /* Fill Linux new_utsname layout from actual OS state. Reject truncation. */
 int (*os_info)(void *,void *out,size_t size);
 /* Final Linux device release, after devres and before its release callback
  * can free the device. Also called during teardown of a faulted adapter. */
 void (*forget)(void *,void *device);
};
#ifdef __cplusplus
extern "C" {
#endif
int pd_tb_native_install(const struct pd_tb_native_ops *);
bool pd_tb_native_ready(void);
#ifdef __cplusplus
}
#endif
#endif
