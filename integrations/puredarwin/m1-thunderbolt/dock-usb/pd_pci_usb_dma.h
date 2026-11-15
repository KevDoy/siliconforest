/* SPDX-License-Identifier: GPL-2.0 */
/* Native PCI xHCI glue; never use the direct USB1 DART for a dock device. */
#ifndef PD_PCI_USB_DMA_H
#define PD_PCI_USB_DMA_H
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>
#include <IOKit/IOService.h>
class PDPCIUSBDMABuffer : public IOBufferMemoryDescriptor {
 OSDeclareDefaultStructors(PDPCIUSBDMABuffer)
 IODMACommand *command;
 IOService *endpoint;
 UInt64 iova;
 UInt32 retiring;
 bool prepared;
 void free() override;
public:
 static PDPCIUSBDMABuffer *allocate(IOService *,UInt64 bytes,UInt64 alignment);
 UInt64 dmaAddress() const {return iova;}
 // The producer must already have completed/halted. Caller retains its own
 // reference across this call: the real command and mapper also retain us.
 // Failure retains command/backing until reboot; never call release after it.
 IOReturn retire();
};
#endif
