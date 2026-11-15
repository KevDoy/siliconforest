/* SPDX-License-Identifier: GPL-2.0 */
/* Policy over an actual domain-locked NHI snapshot. Layout/state from pinned
 * Linux Thunderbolt; no synthetic topology, hardware write or drain proof.
 */
#ifndef PD_TB_RETRAIN_POLICY_H
#define PD_TB_RETRAIN_POLICY_H
#include "pd_tb_nhi.h"
#include <errno.h>
static inline int pd_tb_retrain_peerless(const struct pd_tb_nhi_status *s)
{
 if(!s)return EINVAL;
 if(!s->root_header_valid || s->port_count!=2 || s->cable_info!=0x411)return EOPNOTSUPP;
 unsigned ports=0;
 for(unsigned n=0;n<2;++n){
  const struct pd_tb_nhi_port_status *p=&s->ports[n];
  if(p->port<1 || p->port>2 || (ports&(1u<<p->port)) || !p->flags_valid ||
     !p->cap_phy || p->error)return EIO;
  ports|=1u<<p->port;
  if(p->state!=7 || p->remote || p->remote_authorized || p->lane_disabled || p->adapter_locked)return EBUSY;
 }
 return ports==6?0:EIO;
}
#endif
