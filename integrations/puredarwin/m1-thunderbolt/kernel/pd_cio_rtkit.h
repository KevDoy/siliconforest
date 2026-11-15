/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* ACIO RTKit bridge, no Linux or XNU types across the ABI. */
#ifndef PD_CIO_RTKIT_H
#define PD_CIO_RTKIT_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct pd_cio_rtkit;
/* SRAM remains mapped and owned until successful stop. No firmware is loaded
 * or copied: use the boot firmware's validated __DATA SRAM segment. */
struct pd_cio_rtkit *pd_cio_rtkit_create(void *dt_entry,void *sram,uint64_t phys,size_t span);
int pd_cio_rtkit_boot(struct pd_cio_rtkit *);
int pd_cio_rtkit_stop(struct pd_cio_rtkit *);
/* Actual live engine metadata and M3 FIFO controls only. Caller retains the
 * powered M3 and serializes against boot/stop; no message/FW payload reads. */
struct pd_cio_rtkit_status {
    uint32_t protocol,ap_power,iop_power,running,crashed,shutting_down;
    uint32_t syslog_entries,syslog_message_bytes,syslog_buffer_bytes,syslog_mapped;
    uint32_t syslog_ack_count,syslog_ack_errors,syslog_first_ack_error;
    uint32_t ioreport_ack_count,ioreport_ack_errors,ioreport_first_ack_error;
    uint32_t mailbox_active,a2i_control,i2a_control,mailbox_irq_enable;
    /* RX then TX: registered, disable-depth, taken, handled. */
    uint32_t mailbox_irq_state[8];
};
int pd_cio_rtkit_snapshot(struct pd_cio_rtkit *,struct pd_cio_rtkit_status *);
/* Called only by this kext's private RTKit copy AFTER the real send result.
 * Never substitutes an ACK, retries a message or stores message contents. */
void pd_cio_rtkit_observe_ack(void *engine,unsigned endpoint,int result);
int pd_cio_mailbox_irq_snapshot(int rx_handle,int tx_handle,uint32_t out[8]);
int pd_cio_layer_up(void);
#ifdef __cplusplus
}
#endif
#endif
