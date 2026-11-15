/* SPDX-License-Identifier: GPL-2.0 */
/* Bounded real lane observation using the pinned tb_regs.h layout.
 * Layout from Linux Thunderbolt (Intel Corporation / Andreas Noever).
 * No writes, retries, cached topology, authorization or polling timer.
 */
#ifndef PD_TB_LANE_OBSERVATION_H
#define PD_TB_LANE_OBSERVATION_H
#include <stdint.h>
#include <errno.h>
struct pd_tb_lane_observation {
 uint32_t state,disabled,locked;
};
typedef int (*pd_tb_lane_reader)(void *,uint32_t,uint32_t *,unsigned);
static inline int pd_tb_lane_observe(void *ctx,pd_tb_lane_reader read,
 uint32_t cap,struct pd_tb_lane_observation *out)
{
 uint32_t phy[2],adapter;
 if(!ctx || !read || !out || !cap || cap==UINT32_MAX)return -EINVAL;
 int ret=read(ctx,cap,phy,2);if(ret)return ret;
 ret=read(ctx,4,&adapter,1);if(ret)return ret;
 struct pd_tb_lane_observation result={
  (phy[1]>>26)&15,(phy[1]>>14)&1,(adapter>>31)&1
 };
 *out=result;return 0;
}
#endif
