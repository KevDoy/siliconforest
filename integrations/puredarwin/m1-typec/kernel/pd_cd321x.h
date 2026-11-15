/* SPDX-License-Identifier: GPL-2.0 */
/*
 * CD321x register framing/status bits from Omacom linux-aurora 6a08a08e,
 * drivers/usb/typec/tipd/core.c and tps6598x.h:
 * Copyright (C) 2017, Intel Corporation
 * Author: Heikki Krogerus <heikki.krogerus@linux.intel.com>
 * Copyright (C) The Asahi Linux Contributors
 *
 * This is the register transport, not the Type-C mux/role state machine.
 */
#ifndef PD_CD321X_H
#define PD_CD321X_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/errno.h>

#define PD_TPS_MAX_LEN 64
#define PD_TPS_REG_VID 0x00
#define PD_TPS_REG_MODE 0x03
#define PD_TPS_REG_STATUS 0x1a
#define PD_TPS_REG_DATA_STATUS 0x5f
#define PD_TPS_STATUS_PLUG_PRESENT (1u << 0)
#define PD_TPS_STATUS_UPSIDE_DOWN (1u << 4)
#define PD_TPS_STATUS_PORTROLE (1u << 5)
#define PD_TPS_STATUS_DATAROLE (1u << 6)
#define PD_TPS_DATA_USB2 (1u << 4)
#define PD_TPS_DATA_USB3 (1u << 5)
#define PD_TPS_DATA_DP (1u << 8)
#define PD_TPS_DATA_TBT (1u << 16)
#define PD_CD321X_DATA_HPD (1u << 15)
#define PD_CD321X_DATA_USB4 (1u << 23)

/* Linux's I2C protocol reads len+1 bytes and validates the advertised length.
 * An advertised register size larger than the requested prefix is valid.
 * Leave the caller's destination unchanged on an incomplete/malformed read. */
static inline int pd_cd321x_unpack(const uint8_t *wire, size_t available,
                                  void *dst, size_t len)
{
    if (!wire || !dst || !len || len > PD_TPS_MAX_LEN)
        return EINVAL;
    if (available < len + 1 || wire[0] < len)
        return EIO;
    memcpy(dst, wire + 1, len);
    return 0;
}

static inline uint32_t pd_cd321x_le32(const uint8_t v[4])
{
    return (uint32_t)v[0] | (uint32_t)v[1] << 8 |
           (uint32_t)v[2] << 16 | (uint32_t)v[3] << 24;
}
#endif
