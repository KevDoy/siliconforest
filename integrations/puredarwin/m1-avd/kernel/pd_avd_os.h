/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * What the AVD driver core needs from its surroundings. Implemented by PDAppleAVD.cpp
 * (the kext) and by host-test/host_os.c (a recording stand-in for the hardware).
 */
#ifndef _PD_AVD_OS_H
#define _PD_AVD_OS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#ifdef KERNEL
#include <string.h>
#include <sys/errno.h>
#include <libkern/libkern.h>
#else
#include <string.h>
#include <errno.h>
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void *pd_avd_os_zalloc(size_t bytes);
void  pd_avd_os_free(void *p);
/* level 0 error, 1 warning, 2 info, 3 debug */
void  pd_avd_os_log(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void  pd_avd_os_usleep(uint32_t us);

uint32_t pd_avd_os_readl(const void *addr);
void     pd_avd_os_writel(uint32_t value, void *addr);
void     pd_avd_os_memcpy_toio(void *to, const void *from, size_t bytes);

/* a lock that the completion path (interrupt or polling thread) may take */
void         *pd_avd_os_lock_alloc(void);
void          pd_avd_os_lock_free(void *lock);
unsigned long pd_avd_os_lock(void *lock);
void          pd_avd_os_unlock(void *lock, unsigned long flags);

/*
 * Zeroed device memory: wired, mapped for the decoder at *iova (a multiple of the 16 KiB
 * page size, below 2^38). `cookie` identifies it for free and for mapping into a task.
 */
void *pd_avd_os_dma_alloc(size_t bytes, uint64_t *iova, void **cookie);
void  pd_avd_os_dma_free(void *cookie);

#ifdef __cplusplus
}
#endif
#endif
