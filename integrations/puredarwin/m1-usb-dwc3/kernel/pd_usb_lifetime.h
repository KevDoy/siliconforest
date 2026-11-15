/* SPDX-License-Identifier: MIT */
/* Admission for controller operations while a cable session is being removed.
 * Closing prevents new operations; the owner waits for active==0 before
 * halting/resetting hardware. This does not itself establish DMA quiescence.
 */
#ifndef PD_USB_LIFETIME_H
#define PD_USB_LIFETIME_H
#include <stdint.h>
#define PD_USB_CLOSED (1u << 31)
struct pd_usb_lifetime { uint32_t state; };
static inline void pd_usb_lifetime_init(struct pd_usb_lifetime *g)
{ __atomic_store_n(&g->state, 0, __ATOMIC_RELEASE); }
static inline int pd_usb_lifetime_enter(struct pd_usb_lifetime *g)
{
    uint32_t old = __atomic_load_n(&g->state, __ATOMIC_ACQUIRE);
    for (;;) {
        if (old & PD_USB_CLOSED || old == PD_USB_CLOSED - 1) return 0;
        if (__atomic_compare_exchange_n(&g->state, &old, old + 1, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return 1;
    }
}
static inline void pd_usb_lifetime_leave(struct pd_usb_lifetime *g)
{ __atomic_fetch_sub(&g->state, 1, __ATOMIC_RELEASE); }
static inline void pd_usb_lifetime_close(struct pd_usb_lifetime *g)
{ __atomic_fetch_or(&g->state, PD_USB_CLOSED, __ATOMIC_ACQ_REL); }
static inline uint32_t pd_usb_lifetime_active(const struct pd_usb_lifetime *g)
{ return __atomic_load_n(&g->state, __ATOMIC_ACQUIRE) & ~PD_USB_CLOSED; }
static inline int pd_usb_lifetime_closed(const struct pd_usb_lifetime *g)
{ return !!(__atomic_load_n(&g->state, __ATOMIC_ACQUIRE) & PD_USB_CLOSED); }
#ifdef __cplusplus
class PDUSBOperation {
    struct pd_usb_lifetime *gate;
public:
    explicit PDUSBOperation(struct pd_usb_lifetime *g) : gate(pd_usb_lifetime_enter(g) ? g : nullptr) {}
    ~PDUSBOperation() { if (gate) pd_usb_lifetime_leave(gate); }
    explicit operator bool() const { return gate != nullptr; }
    PDUSBOperation(const PDUSBOperation &) = delete;
    PDUSBOperation &operator=(const PDUSBOperation &) = delete;
};
#endif
#endif
