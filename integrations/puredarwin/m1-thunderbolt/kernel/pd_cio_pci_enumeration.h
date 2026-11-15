/* SPDX-License-Identifier: MIT */
#ifndef PD_CIO_PCI_ENUMERATION_H
#define PD_CIO_PCI_ENUMERATION_H
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include "pd_cio_pcie_controller.h"
#include "pd_cio_pcie_host.h"
#ifdef PD_CIO_ENDPOINT_MAPPER
#include "pd_cio_pci_mapper.h"
#endif
/* Private, enumeration-only profile. No matching personality or DMA/MSI
 * admission. prepare/activated/deactivate are actual Linux CM hooks.
 * publishAfterApproval is called ONLY after real TS3 approval returns,
 * outside the domain lock and after the post-activation callback returned.
 * A failed/removed programmed host retains resources until reboot. This is
 * not hot-remove support and cannot satisfy the ten-cycle milestone.
 */
class PDCIOPCIEnumeration : public OSObject {
 OSDeclareDefaultStructors(PDCIOPCIEnumeration)
 IOService *owner,*nub;
 void *context;
 bool (*powerProof)(void *);
 IOLock *lock;
 PDCIOPCIController *controller;
 PDCIOPCIHost *host;
 uint32_t posted,committed,attempted,revoked;
 bool preparing;
#ifdef PD_CIO_ENDPOINT_MAPPER
 PDCIOPCIDartMapper *i210Mapper;
 bool mapperAttempted;
#endif
 static bool power(void *);
 static bool tunnel(void *);
 int catalogueSafe();
 int fail(int);
public:
 bool initWithOwner(IOService *,void *,bool (*)(void *));
 int checkProfile(); // real catalogue metadata only, no PCIe MMIO
 int prepare();
 int activated();
 int deactivate();
 int publishAfterApproval();
#ifdef PD_CIO_ENDPOINT_MAPPER
 int prepareI210Mapper(); // Actual IOMapper only; no BME or endpoint admission.
#endif
 bool blocksStop() const;
 bool pristineForRetrain(); // No reserved controller or attempted host.
 void free() override;
};
#endif
