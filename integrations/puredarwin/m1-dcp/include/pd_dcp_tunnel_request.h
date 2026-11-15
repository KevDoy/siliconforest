/* SPDX-License-Identifier: MIT */
/* Private native cross-kext ABI. No Linux device/PHY/connector structs. */
#ifndef PD_DCP_TUNNEL_REQUEST_H
#define PD_DCP_TUNNEL_REQUEST_H
#include <stdbool.h>
#include <stdint.h>
#define PD_DCP_TUNNEL_VERSION 1u
#define PD_DCP_TUNNEL_FUNCTION "PDDcpThunderboltTunnel"
struct pd_dcp_tunnel_request {
    uint32_t version, size, typec_port, dpin;
    uint64_t generation;
    uint32_t active, reserved;
    int (*set_active)(void *context,uint64_t generation,bool active);
    void *context;
};
#if defined(__cplusplus)
static_assert(sizeof(pd_dcp_tunnel_request)==48,"native tunnel request ABI");
#else
_Static_assert(sizeof(struct pd_dcp_tunnel_request)==48,"native tunnel request ABI");
#endif
#endif
