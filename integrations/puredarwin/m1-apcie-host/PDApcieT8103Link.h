/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Apple T8103 PCIe controller, PHY and port 0 link bring-up for PureDarwin.
 * See PDApcieT8103Link.cpp for provenance (m1n1, MIT; Linux pcie-apple.c, GPL-2.0).
 */
#ifndef _PDAPCIET8103LINK_H
#define _PDAPCIET8103LINK_H

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOReturn.h>
#include <libkern/c++/OSArray.h>

/*
 * Make sure the port 0 link of /arm-io/apcie is up.
 *
 * apcie: the /arm-io/apcie device-tree entry.
 * regs:  the array IODTResolveAddressing(apcie, "reg", NULL) returned. The caller keeps
 *        it; this function never resolves the apcie "reg" again (that would replace the
 *        registry-owned array under the caller).
 * linksts: receives the last PORT_LINKSTS value that was read, 0 if the port was never read.
 *
 * Nothing is written when the link is already up. When the controller looks initialised
 * (as after m1n1 pcie_init) only the port link sequence runs. Otherwise the whole m1n1
 * controller/PHY/port bring-up runs first. Boot argument pdapcielink=2 forces the whole
 * bring-up whenever the link is down; pdapcielink=0 is handled by the caller (no bring-up).
 *
 * The port link sequence powers the radio itself through the SMC (key gP0d, see
 * PDAppleSMC.h) after asserting PERST#. Boot argument pdapciesmc: 0 = no SMC access at
 * all (the launcher must have powered the radio); 1 = default; +2 = write gP0d even when
 * it already reads as enabled; +4 = also send the kdebug endpoint start message.
 *
 * Returns kIOReturnSuccess only with PORT_LINKSTS_UP set. Every failure is logged.
 */
IOReturn PDApcieT8103LinkBringUp(IORegistryEntry *apcie, OSArray *regs, UInt32 *linksts);

/*
 * The reverse: port 0 and the PHY are stopped and, with kPDApcieShutdownRadioOff, the radio's
 * power is switched off through the SMC (not with pdapciesmc=0). See Link::shutdown() in
 * PDApcieT8103Link.cpp for the order. Nothing calls this during a normal boot. The caller
 * must have stopped every use of the devices behind the port; afterwards their BARs and
 * config space must not be touched until PDApcieT8103LinkBringUp() succeeded again.
 *
 * *portDown is set once the port is down (also when it already was). The return value is
 * that of the last step, so a failed radio power write is reported with the port down.
 *
 * UNTESTED on hardware, and the SMC "off" value (PD_SMC_RADIO_POWER_OFF) is unverified.
 */
enum {
    kPDApcieShutdownRadioOff = 1u << 0,
};
IOReturn PDApcieT8103PortShutdown(IORegistryEntry *apcie, OSArray *regs, UInt32 flags, bool *portDown);

#endif
