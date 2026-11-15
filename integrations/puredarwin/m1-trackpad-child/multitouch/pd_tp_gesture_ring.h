/* SPDX-License-Identifier: MIT */
/* DESK K14 wire ABI. Read under the producer lock; no user pointers. */
#ifndef PD_TP_GESTURE_RING_H
#define PD_TP_GESTURE_RING_H
#include <stdint.h>
#define PD_TP_SWIPE_RING 32u
struct pd_tp_swipe_event {
    uint32_t seq;
    uint8_t phase, fingers;
    int16_t dx, dy;
    uint8_t synthetic, kind; /* 0 swipe, 1 pinch */
    uint32_t ms;
    uint32_t scale_q16;
    int32_t angle_cdeg;
};
struct pd_tp_swipe_ring {
    uint32_t version, next_seq;
    struct pd_tp_swipe_event ev[PD_TP_SWIPE_RING];
};
#if defined(__cplusplus)
static_assert(sizeof(pd_tp_swipe_event)==24 && sizeof(pd_tp_swipe_ring)==776,"K14 gesture ABI");
#else
_Static_assert(sizeof(struct pd_tp_swipe_event)==24 && sizeof(struct pd_tp_swipe_ring)==776,"K14 gesture ABI");
#endif
#endif
