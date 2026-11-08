/* Host-test stand-in for <mach/mach.h>: only what PDGOP.c uses. */
#ifndef PDGOP_HOSTTEST_MACH_H
#define PDGOP_HOSTTEST_MACH_H
#include <mach/kern_return.h>
#define MACH_PORT_NULL 0u
unsigned int mach_task_self(void);
kern_return_t mach_port_deallocate(unsigned int task, unsigned int name);
#endif
