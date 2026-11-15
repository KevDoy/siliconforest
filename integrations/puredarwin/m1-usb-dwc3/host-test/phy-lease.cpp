/* SPDX-License-Identifier: MIT */
#include "pd_usbc_phy_lease.h"
#include <cassert>
#include <cstdio>
int main()
{
 int a,b;pd_usbc_phy_lease s={};uint64_t token=0;
 assert(pd_usbc_phy_claim(&s,&a,0,PD_USBC_PHY_TBT,&token)==ESTALE);
 assert(!pd_usbc_phy_observe(&s,1,PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_TBT),false));
 for (unsigned i=0;i<10000;++i) {
  auto epoch=s.epoch;
  assert(!pd_usbc_phy_claim(&s,&a,epoch,PD_USBC_PHY_TBT,&token));
  assert(pd_usbc_phy_claim(&s,&b,epoch,PD_USBC_PHY_TBT,&token)==EBUSY);
  assert(pd_usbc_phy_commit(&s,&b,token,epoch)==ESTALE);
  assert(!pd_usbc_phy_commit(&s,&a,token,epoch));
  assert(!pd_usbc_phy_live(&s,&a,token));
  assert(!pd_usbc_phy_observe(&s,s.cable_key,s.eligible,s.reversed));
  assert(s.phase==PD_USBC_PHY_LIVE && s.epoch==epoch);
  assert(!pd_usbc_phy_observe(&s,s.cable_key+1,0,!s.reversed));
  assert(s.owner==&a && s.phase==PD_USBC_PHY_REVOKED);
  assert(pd_usbc_phy_live(&s,&a,token)==EBUSY);
  assert(pd_usbc_phy_release(&s,&b,token,0,false)==ESTALE);
  assert(pd_usbc_phy_release(&s,&a,token,EBUSY,false)==EBUSY);
  assert(s.owner==&a);
  assert(!pd_usbc_phy_release(&s,&a,token,0,false));
  assert(pd_usbc_phy_live(&s,&a,token)==ESTALE);
  assert(!pd_usbc_phy_observe(&s,s.cable_key+1,PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_TBT),false));
  uint64_t next;
  assert(!pd_usbc_phy_claim(&s,&b,s.epoch,PD_USBC_PHY_TBT,&next));
  assert(next>token);
  assert(!pd_usbc_phy_observe(&s,s.cable_key+1,s.eligible,true));
  assert(pd_usbc_phy_commit(&s,&b,next,s.epoch-1)==ESTALE);
  assert(!pd_usbc_phy_release(&s,&b,next,0,false));
 }
 assert(!pd_usbc_phy_claim(&s,&a,s.epoch,PD_USBC_PHY_TBT,&token));
 s.epoch=UINT64_MAX;
 assert(pd_usbc_phy_observe(&s,s.cable_key+1,0,false)==EOVERFLOW);
 assert(s.quarantined && s.phase==PD_USBC_PHY_REVOKED && s.owner==&a);
 assert(!pd_usbc_phy_release(&s,&a,token,0,false));
 assert(pd_usbc_phy_claim(&s,&a,s.epoch,PD_USBC_PHY_TBT,&token)==EIO);
 s={};assert(!pd_usbc_phy_observe(&s,1,PD_USBC_PHY_MODE_BIT(PD_USBC_PHY_DP),false));
 s.serial=UINT64_MAX;
 assert(pd_usbc_phy_claim(&s,&a,s.epoch,PD_USBC_PHY_DP,&token)==EOVERFLOW);
 assert(s.quarantined && !s.owner);
 s={};assert(!pd_usbc_phy_observe(&s,1,0,false));
 assert(pd_usbc_phy_claim(&s,&a,s.epoch,PD_USBC_PHY_DP,&token)==ENODEV);
 puts("10000 PHY lease revocation/retry/stale-token cycles; failed drain and counter-overflow quarantine passed (metadata only)");
}
