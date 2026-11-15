/* SPDX-License-Identifier: MIT */
/* Saturating, nonblocking diagnostic budget; no timer or retry worker. */
#ifndef PD_I210_TRACE_H
#define PD_I210_TRACE_H
#include <stdint.h>
#include <stdbool.h>
static inline bool pd_i210_trace_take(uint32_t *budget)
{
    uint32_t value = __atomic_load_n(budget, __ATOMIC_RELAXED);
    for (unsigned attempt = 0; value < 8 && attempt < 8; ++attempt)
        if (__atomic_compare_exchange_n(budget, &value, value + 1, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return true;
    return false;
}
#endif
