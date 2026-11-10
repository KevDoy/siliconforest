// SPDX-License-Identifier: GPL-2.0
/*
 * PDRustHello: the C++ IOKit shell around a Rust static library (rust/pdrusthello).
 * Matches IOResources, and start() runs the Rust self-test once and logs what it returned.
 * The Rust side imports pd_rust_log and pd_rust_panic from here.
 */
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <kern/debug.h>

#define PD_RUST_HELLO_LOG(...) IOLog(__VA_ARGS__)
#include "pd_rust_hello.h"

class PDRustHello : public IOService
{
    OSDeclareDefaultStructors(PDRustHello)

public:
    bool start(IOService *provider) override;
};

#define super IOService
OSDefineMetaClassAndStructors(PDRustHello, IOService)

extern "C" void
pd_rust_log(const char *text, unsigned long len)
{
    IOLog("PDRustHello: %.*s\n", (int)len, text);
}

extern "C" void
pd_rust_panic(const char *text, unsigned long len)
{
    panic("PDRustHello: rust panic: %.*s", (int)len, text);
}

bool PDRustHello::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    IOLog("PDRustHello: start\n");
    bool ok = pd_rust_hello_run();
    setProperty("PDRustHelloResult", ok ? "ok" : "failed");
    registerService();
    return true;
}
