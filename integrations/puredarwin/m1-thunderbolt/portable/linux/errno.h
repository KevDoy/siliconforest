/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_PORTABLE_ERRNO_H
#define PD_TB_PORTABLE_ERRNO_H
#ifdef __linux__
#include_next <linux/errno.h>
#else
#include <sys/errno.h>
#endif
#endif
