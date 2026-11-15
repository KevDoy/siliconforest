/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_UTSNAME_H
#define PD_TB_UTSNAME_H
#include <uapi/linux/utsname.h>
/* Required native OS-info snapshot; no fabricated hostname implementation. */
struct new_utsname *pd_tb_utsname(void);
#define utsname pd_tb_utsname
#endif
