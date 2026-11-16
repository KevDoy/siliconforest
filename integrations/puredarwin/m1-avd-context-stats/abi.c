/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include <stdio.h>
#include <pdavd/pdavd_ioctl.h>
_Static_assert(sizeof(struct pdavd_info)==64,"old info");
_Static_assert(sizeof(struct pdavd_create)==64,"old create");
_Static_assert(sizeof(struct pdavd_map)==24,"old map");
_Static_assert(sizeof(struct pdavd_decode)==176,"old decode");
_Static_assert(sizeof(struct pdavd_destroy)==4,"old destroy");
_Static_assert(sizeof(struct pdavd_import)==528,"old import");
#ifdef __APPLE__
_Static_assert(PDAVD_IOC_INFO==0x40405601UL,"old Darwin info");
_Static_assert(PDAVD_IOC_CREATE==0xc0405602UL,"old Darwin create");
_Static_assert(PDAVD_IOC_MAP==0xc0185603UL,"old Darwin map");
_Static_assert(PDAVD_IOC_DECODE==0xc0b05604UL,"old Darwin decode");
_Static_assert(PDAVD_IOC_DESTROY==0x80045605UL,"old Darwin destroy");
_Static_assert(PDAVD_IOC_IMPORT==0x82105606UL,"old Darwin import");
#endif
#ifdef PDAVD_CAP_CONTEXT_STATS
_Static_assert(sizeof(struct pdavd_context_stats)==128,"context stats ABI");
_Static_assert(offsetof(struct pdavd_context_stats,generation)==16,"generation");
_Static_assert(offsetof(struct pdavd_context_stats,jobs)==24,"jobs");
#ifdef __APPLE__
_Static_assert(PDAVD_IOC_CONTEXT_STATS==0xc0805607UL,"Darwin ioctl");
#endif
#endif
int main(void){printf("%zu %zu %zu %zu %zu %zu %lu %lu %lu %lu %lu %lu\n",sizeof(struct pdavd_info),sizeof(struct pdavd_create),sizeof(struct pdavd_map),sizeof(struct pdavd_decode),sizeof(struct pdavd_destroy),sizeof(struct pdavd_import),(unsigned long)PDAVD_IOC_INFO,(unsigned long)PDAVD_IOC_CREATE,(unsigned long)PDAVD_IOC_MAP,(unsigned long)PDAVD_IOC_DECODE,(unsigned long)PDAVD_IOC_DESTROY,(unsigned long)PDAVD_IOC_IMPORT);return 0;}
