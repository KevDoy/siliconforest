/* SPDX-License-Identifier: MIT */
/*
 * What the built-in keyboard's driver (PDM1SPIHID) and the Touch Bar's (PDAppleTouchBar)
 * tell each other, as IOService messages: neither kext links against the other, each
 * finds the other's service by class name.
 */
#ifndef _PD_TOUCHBAR_KBD_H
#define _PD_TOUCHBAR_KBD_H

#include <IOKit/IOMessage.h>

/*
 * PDM1SPIHID -> PDAppleTouchBar, for a keyboard input report: the argument's bit 0 is the
 * state of the Fn key, bit 1 says the report is keyboard activity (always set), bit 2 that
 * the Fn key's place in the report is known.
 */
#define kPDTouchBarKeyboardMessage      iokit_vendor_specific_msg(0x5442)
#define kPDTouchBarKeyboardFn           0x1
#define kPDTouchBarKeyboardActivity     0x2
#define kPDTouchBarKeyboardFnKnown      0x4

/*
 * PDAppleTouchBar -> PDM1SPIHID, for tests: make up a keyboard input report with no key
 * down and the Fn bit as the argument's bit 0 says, and look at it as at a report the
 * keyboard sent (nothing is delivered to IOHIDFamily). kIOReturnUnsupported when the
 * keyboard's descriptor has no Fn usage.
 */
#define kPDTouchBarTestKeyboardReport   iokit_vendor_specific_msg(0x5443)

#define kPDTouchBarServiceClass         "PDAppleTouchBar"
#define kPDKeyboardServiceClass         "PDM1SPIHID"

#endif
