/* SPDX-License-Identifier: GPL-2.0 */
/* Native, explicitly created I210-only SID0 mapper. No personality or BME
 * admission. Fixed DART layout from pinned apple-dart.c; table implementation
 * by Asahi Linux Contributors (GPL-2.0-only OR MIT). Not in the boot profile.
 * All mappings are distinct, whole-page IOBufferMemoryDescriptor bounces.
 * Stop/revoke quarantines until reboot; endpoint/IRQ/work drain is not yet
 * implemented, so this is not hot-remove or Ethernet qualification.
 */
#ifndef PD_CIO_PCI_MAPPER_H
#define PD_CIO_PCI_MAPPER_H
#include "pd_cio_pcie_controller.h"
#include "pd_pciec_dart_sid0.h"
#include "pd_dart_table.h"
#include <IOKit/IOMapper.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
class PDCIOPCIDartMapper : public IOMapper {
 OSDeclareDefaultStructors(PDCIOPCIDartMapper)
 PDCIOPCIController *controller;
 IOPCIDevice *endpoint;
 IOLock *lock;
 struct Table { IOBufferMemoryDescriptor *memory;uint64_t physical; } tables[16];
 // The live command owns its descriptor. We retain the page backing
 // independently, and compare the command's identity at normal retirement.
 struct Mapping { IOBufferMemoryDescriptor *memory;IODMACommand *command;uint64_t address,bytes; } *mappings;
 unsigned tableCount,mappingCount;
 uint8_t *busy;
 pd_dart_tables pt;
 pd_pciec_dart_sid0_io io;
 bool setupAttempted,ready,touched,poisoned,revoked;
 bool inventoryBlocked();
 bool live(); // Called with mapper and controller locks held.
 void poison();
 static uint64_t *table(void *,uint64_t);
 static uint64_t allocateTable(void *);
 static int read(void *,uint32_t,uint32_t *);
 static int write(void *,uint32_t,uint32_t);
 static void delay(void *,unsigned);
 static int publish(void *);
 bool initHardware(IOService *) override;
public:
 // Factory caller attaches/starts this service, then may put this actual
 // object in the endpoint's iommu-parent. No caller in the CM uses it yet.
 bool initWithController(PDCIOPCIController *,IOPCIDevice *);
 uint64_t getPageSize() const override;
 IOReturn iovmMapMemory(IOMemoryDescriptor *,uint64_t,uint64_t,uint32_t,
  const IODMAMapSpecification *,IODMACommand *,const IODMAMapPageList *,
  uint64_t *,uint64_t *) override;
 IOReturn iovmUnmapMemory(IOMemoryDescriptor *,IODMACommand *,uint64_t,uint64_t) override;
 IOReturn iovmInsert(uint32_t,uint64_t,uint64_t,uint64_t,uint64_t) override;
 uint64_t mapToPhysicalAddress(uint64_t) override;
 void stop(IOService *) override;
 void free() override;
};
#endif
