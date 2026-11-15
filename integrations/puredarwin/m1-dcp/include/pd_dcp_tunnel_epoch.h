/* SPDX-License-Identifier: MIT */
/* PDDcp-private bridge storage, NEVER passed to CIO. Caller serialization and
 * actual retained local connector/owner references are required. Each object
 * has one immutable generation/callback identity; retire never recycles it.
 */
#ifndef PD_DCP_TUNNEL_EPOCH_H
#define PD_DCP_TUNNEL_EPOCH_H
#include "pd_dcp_tunnel_request.h"
enum pd_dcp_tunnel_state { PD_DCP_TUNNEL_NEW,PD_DCP_TUNNEL_BOUND,
    PD_DCP_TUNNEL_ACTIVE,PD_DCP_TUNNEL_RETIRED,PD_DCP_TUNNEL_FAILED };
struct pd_dcp_tunnel_epoch {
    void *connector; /* PDDcp's own retained device_node, not CIO's node. */
    const void *owner;
    uint64_t generation;
    uint32_t dpin, state;
    int (*set_active)(void *,uint64_t,bool);
    void *context;
    bool accepting, closing;
};
#ifdef __cplusplus
extern "C" {
#endif
/* Connector/owner lifetime and provider power are established by the native
 * provider BEFORE init/call. These functions publish no provider/route/PHY.
 * The real provider must serialize calls, retain all storage while Linux owns
 * callbacks, and detach/drain before power loss or object reclamation.
 */
int pd_dcp_tunnel_epoch_init(struct pd_dcp_tunnel_epoch *,void *connector,const void *owner);
int pd_dcp_tunnel_epoch_call(struct pd_dcp_tunnel_epoch *,const void *owner,
                            const struct pd_dcp_tunnel_request *);
#ifdef __cplusplus
}
#endif
#endif
