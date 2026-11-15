/* SPDX-License-Identifier: MIT */
#ifndef PDAVE_IOCTL_H
#define PDAVE_IOCTL_H
#include <stdint.h>
#ifdef __APPLE__
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#define PDAVE_ABI_VERSION 1
#define PDAVE_INFO_PRELOADED (1u<<0)
#define PDAVE_INFO_OUTSIDE_VM (1u<<1)
#define PDAVE_INFO_TEXT_IDENTITY (1u<<2)
#define PDAVE_INFO_COLD_RESERVED (1u<<3)
#define PDAVE_INFO_ENCODER_READY (1u<<4)
#define PDAVE_CAP_H264 (1u<<0)
/* Diagnostic INFO is implemented first. No encoder capability until real
 * power/DMA/IPC/session prerequisites are established. No physical addresses. */
struct pdave_info {
    uint32_t abi, size, flags, caps;
    uint32_t soc, segments, error, reserved;
    uint32_t segment_size[2], segment_flags[2];
    char firmware[48], rtkit[48], iboot[48];
};
#define PDAVE_IOC_INFO _IOR('E',1,struct pdave_info)
#endif
