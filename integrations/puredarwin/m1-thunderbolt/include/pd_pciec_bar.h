/* SPDX-License-Identifier: MIT */
/* T8103 apciec1 ranges: pinned linux-aurora t8103.dtsi, not Wi-Fi apcie. */
#ifndef PD_PCIEC_BAR_H
#define PD_PCIEC_BAR_H
#include <stdint.h>
#include <errno.h>
static inline int pd_pciec_bar_cpu(uint64_t bus,uint64_t bytes,uint64_t *cpu)
{
 if(!cpu || !bytes)return EINVAL;
 uint64_t end,offset;
 if(bus>=0x00100000 && bus<0x40000000){end=0x40000000;offset=0x600000000ULL;}
 else if(bus>=0x40000000 && bus<0x80000000){end=0x80000000;offset=0x600000000ULL;}
 else if(bus>=0x580000000ULL && bus<0x600000000ULL){end=0x600000000ULL;offset=0;}
 else return ERANGE;
 if(bytes>end-bus)return ERANGE;
 *cpu=bus+offset;return 0;
}
#endif
