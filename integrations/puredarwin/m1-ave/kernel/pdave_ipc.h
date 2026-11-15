/* SPDX-License-Identifier: GPL-2.0-only */
/* Polling T8103/13.5 transport port of upstream ave_ipc.c. See ../NOTICE.
 * Caller serializes all calls and owns mapped/coherent DMA until a real halt.
 * This module does NOT power/start the core, allocate/map DMA or reserve DATA.
 * It is not yet attached to the diagnostic service. */
#ifndef PDAVE_IPC_H
#define PDAVE_IPC_H
#include <stddef.h>
#include "pdave/pdave_preload.h"
#include "upstream/ave_abi_boot.h"
struct pdave_ipc_ops {
 void *context;
 int (*read)(void *,uint32_t,uint32_t *); /* offsets in SVE bank only */
 int (*write)(void *,uint32_t,uint32_t);
 void (*delay_us)(void *,uint32_t);
 void (*barrier)(void *); /* real coherent DMA + device ordering */
};
enum pdave_ipc_phase {PDAVE_IPC_IDLE,PDAVE_IPC_CONFIGURED,PDAVE_IPC_READY,PDAVE_IPC_FAILED};
struct pdave_ipc_message {uint64_t firmware;void *payload;uint32_t bytes,flags;};
struct pdave_ipc {
 struct pdave_ipc_ops ops;
 unsigned char *cpu;
 uint64_t iova,fw_base,heap_iova;
 uint32_t bytes,heap_bytes,client_bytes;
 enum pdave_ipc_phase phase;
 struct ave_ring ring[AVE_CH_MAX];
 uint32_t bound,doorbell[AVE_CH_MAX];
 struct pdave_ipc_message pending_reply;
 unsigned reply_pending;
};
/* DMA must already be exclusive, coherent, mapped and 16KiB aligned. Only
 * validated cold-reserved preload is accepted; validation cannot prove that
 * the platform's reservation assertion is true. No device I/O on init. */
int pdave_ipc_init(struct pdave_ipc *,const struct pdave_preload *,
 const struct pdave_ipc_ops *,void *,uint64_t,uint32_t,uint64_t,uint32_t);
int pdave_ipc_configure(struct pdave_ipc *);
/* Call after the platform starts the core. Timeouts/faults quarantine state;
 * nothing here frees DMA while a core might still reference it. */
int pdave_ipc_handshake(struct pdave_ipc *);
void *pdave_ipc_from_fw(struct pdave_ipc *,uint64_t,size_t);
/* Post-handshake rings. Caller copies a completion before returning its
 * credit with ack; receiving alone does not let firmware reuse its buffer. */
int pdave_ipc_send(struct pdave_ipc *,unsigned channel,uint64_t payload_iova,uint32_t bytes,uint32_t flags);
int pdave_ipc_receive(struct pdave_ipc *,unsigned channel,struct pdave_ipc_message *);
int pdave_ipc_ack(struct pdave_ipc *,unsigned channel,const struct pdave_ipc_message *);
#endif
