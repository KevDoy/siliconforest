/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_PORTABLE_BITS_H
#define PD_TB_PORTABLE_BITS_H
#define fallthrough __attribute__((fallthrough))
#define BIT(n) (1UL << (n))
#endif
