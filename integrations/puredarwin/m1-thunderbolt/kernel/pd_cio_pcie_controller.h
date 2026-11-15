/* SPDX-License-Identifier: GPL-2.0 */
/* Native adapter for pd_pciec_port, following pinned pcie-apple.c.
 * Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
 * Copyright (C) 2021 Google LLC
 * Copyright (C) 2021 Corellium LLC
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */
#ifndef PD_CIO_PCIE_CONTROLLER_H
#define PD_CIO_PCIE_CONTROLLER_H
#include "pd_soc.h"
#include "pd_pciec_port.h"
#include <IOKit/IOLocks.h>
#include <IOKit/IOService.h>
/* Unwired native controller backend: no matching personality, DART access,
 * endpoint publication, MSI or DMA. The future CM adapter must own the actual
 * ACIO/PHY lease and supply real power and activated-tunnel proofs. Never run
 * coldInit or linkStart while holding the CM domain lock: its config traffic
 * must run. BOTH require a real activated-tunnel proof, after the CM's
 * post-activation callback has returned, matching Linux child population.
 * Any attempted hardware programming pins ownership/mappings until reboot;
 * revoke blocks new accesses but supplies NO hardware or child drain proof.
 */
class PDCIOPCIController : public OSObject {
 OSDeclareDefaultStructors(PDCIOPCIController)
 IOService *owner;
 void *context;
 bool (*powerProof)(void *),(*tunnelProof)(void *);
 IOLock *lock;
 PDMmio windows[5],powerGate;
 bool admitted,touched,poisoned,cold,linked;
 pd_pciec_io io;
 bool live();
 static int read(void *,pd_pciec_window,uint32_t,uint32_t *);
 static int write(void *,pd_pciec_window,uint32_t,uint32_t);
 static int tune(void *,pd_pciec_window);
 static void delay(void *,unsigned);
public:
 bool initWithOwner(IOService *,void *,bool (*)(void *),bool (*)(void *));
 int coldInit();
 int linkStart();
 int revoke();
 void free() override;
};
#endif
