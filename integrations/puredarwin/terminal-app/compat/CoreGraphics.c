// SPDX-License-Identifier: MIT
// CoreGraphics additions (wrapper over the Cocoa stream's CoreGraphics, which is re-exported):
// what Terminal.app imports and that library does not have. To be folded into the framework's
// own source, where the context getters can read the real state.
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct CGContext *CGContextRef;
typedef int32_t CGError;
enum { kCGErrorSuccess = 0, kCGErrorFailure = 1000, kCGErrorNotImplemented = 1004 };

// Text rendering state. Onyx2D rasterises glyphs with FreeType's grey anti-aliasing and has
// no LCD ("font smoothing") path; these answer what the renderer does, not what was last set.
bool CGContextGetAllowsAntialiasing(CGContextRef c) { return true; }
bool CGContextGetAllowsFontSmoothing(CGContextRef c) { return false; }
bool CGContextGetShouldSmoothFonts(CGContextRef c) { return false; }
int CGContextGetFontSmoothingStyle(CGContextRef c) { return 0; }
void CGContextSetFontSmoothingStyle(CGContextRef c, int style) { }

// Window server connection calls (private CGS/CPS). The window server here is a Wayland
// compositor: there is no connection number, a client cannot blur what is behind its window,
// and no client can take or give away the keyboard focus. Keystrokes reach only the focused
// client, so "secure event input" has nothing to switch on.
int CGSMainConnectionID(void) { return 0; }
CGError CGSSetSecureEventInput(int connection, bool enable) { return kCGErrorNotImplemented; }
CGError CGSSetWindowBackgroundBlurRadiusWithOpacityHint(int connection, int window, int radius, bool opaque) { return kCGErrorNotImplemented; }
struct SFProcessSerialNumber { uint32_t high, low; };
CGError CPSGetCurrentProcess(struct SFProcessSerialNumber *psn) { if (psn) { psn->high = 0; psn->low = 2; /* kCurrentProcess */ } return kCGErrorSuccess; }
CGError CPSStealKeyFocus(struct SFProcessSerialNumber *psn) { return kCGErrorNotImplemented; }
CGError CPSReleaseKeyFocus(struct SFProcessSerialNumber *psn) { return kCGErrorNotImplemented; }
