// SPDX-License-Identifier: MIT
#include "pd_atc_pipe.h"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cstdio>
struct Model {
 uint32_t regs[9]={0,0,0,0x22,0,0,0,0x30,0x1332};unsigned writes=0,stuck=0;
 static uint32_t read(void *p,pd_atc_window w,uint32_t off){auto m=(Model *)p;assert(w==PD_ATC_PIPE && off<=0x20 && !(off&3));return m->regs[off/4];}
 static void write(void *p,pd_atc_window w,uint32_t off,uint32_t value){auto m=(Model *)p;assert(w==PD_ATC_PIPE && (off==0x1c || off==0x20));++m->writes;if(m->writes!=m->stuck)m->regs[off/4]=value;assert(!(m->regs[7]&1));}
 pd_atc_io io(){return {this,read,write,nullptr,nullptr};}
};
int main(){
 pd_atc_pipe_state state={PD_ATC_DUMMY,false};
 {Model m;auto io=m.io();assert(!pd_atc_pipe_bootstrap_dummy(&io,&state,1));assert(m.writes==1 && m.regs[7]==0x30 && m.regs[8]==0x9332);assert(!pd_atc_pipe_bootstrap_dummy(&io,&state,1) && m.writes==1);}
 {Model m;auto io=m.io();assert(pd_atc_pipe_bootstrap_dummy(&io,&state,0)==EBUSY && !m.writes);}
 for(unsigned i=0;i<4;i++){
  Model m;auto io=m.io();unsigned off[]={3,5,7,8};m.regs[off[i]]=UINT32_MAX;
  assert(pd_atc_pipe_bootstrap_dummy(&io,&state,1)==EIO && !m.writes);
 }
 for(unsigned i=0;i<3;i++){
  Model m;auto io=m.io();if(i==0)m.regs[3]=8;if(i==1)m.regs[5]=1;if(i==2)m.regs[7]|=1;
  assert(pd_atc_pipe_bootstrap_dummy(&io,&state,1)==EBUSY && !m.writes);
 }
 {Model m;m.stuck=1;auto io=m.io();assert(pd_atc_pipe_bootstrap_dummy(&io,&state,1)==EIO && m.writes==1);}
 {Model m;m.regs[8]|=1u<<22;auto io=m.io();assert(!pd_atc_pipe_bootstrap_dummy(&io,&state,1) && m.regs[8]==(0x9332|(1u<<22)) && m.regs[7]==0x30);}
 for(unsigned i=0;i<2;i++){Model m;auto io=m.io();if(i)m.regs[8]&=~0x1000u;else m.regs[8]=(m.regs[8]&~0xfu)|3u;assert(pd_atc_pipe_bootstrap_dummy(&io,&state,1)==EBUSY && !m.writes);}
 state.dummy_pending=true;{Model m;auto io=m.io();assert(pd_atc_pipe_bootstrap_dummy(&io,&state,1)==EBUSY && !m.writes);}
 puts("HOST ONLY: parked PIPE baseline, physical-reset/ACIO proof refusals, inaccessible MMIO and failed readbacks pass");
}
