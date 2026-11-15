// SPDX-License-Identifier: MIT
#include "pd_cio_tunables.h"
#include <map>
#include <string>
#include <array>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cerrno>
struct Model {
    std::map<std::string,std::array<uint8_t,24>> props;
    std::map<std::pair<unsigned,unsigned>,unsigned> regs;
    std::vector<std::string> names;unsigned writes=0,reads=0;
    static const void *get(void *ctx,const char *path,const char *name,size_t *len){auto &m=*(Model*)ctx;
        assert(!strcmp(path,"/arm-io/acio1"));m.names.push_back(name);
        auto &v=m.props[name];v={};v[4]=4;v[8]=0xf;v[16]=0x80;*len=24;return v.data();}
    static uint32_t read(void *ctx,pd_cio_window w,uint32_t off){auto &m=*(Model*)ctx;
        assert((w==PD_CIO_RC&&off<0xc000)||(w==PD_CIO_NHI&&off<0x100000)||(w==PD_CIO_PCIE_ADAPTER&&off<0x4000));
        ++m.reads;return m.regs[{w,off}];}
    static void write(void *ctx,pd_cio_window w,uint32_t off,uint32_t value){auto &m=*(Model*)ctx;
        m.regs[{w,off}]=value;++m.writes;}
    pd_cio_tunable_io io(){return {this,get,read,write};}
};
int main(){
    Model m;auto io=m.io();pd_cio_tunables b={};assert(!pd_cio_tunables_load(&b,&io) && !m.writes);
    for(auto w:{PD_CIO_RC,PD_CIO_NHI,PD_CIO_PCIE_ADAPTER})assert(!pd_cio_tunables_apply(&b,&io,w));
    assert(m.writes==11 && m.regs.size()==11);
    for(unsigned off:{0u,0x4000u,0x8000u})assert(m.regs.at({PD_CIO_RC,off})==0x80);
    for(unsigned off:{0xf0000u,0xec000u,0xe8000u,0xf4000u,0xf8000u,0xfc000u,0x4000u})assert(m.regs.at({PD_CIO_NHI,off})==0x80);
    assert(m.regs.at({PD_CIO_PCIE_ADAPTER,0})==0x80);
    // clear-mask and set are independent, matching Linux (set may be outside clear).
    for(auto w:{PD_CIO_RC,PD_CIO_NHI,PD_CIO_PCIE_ADAPTER})assert(!pd_cio_tunables_apply(&b,&io,w));
    assert(m.writes==11); // unchanged writes skipped
    pd_cio_tunable_observation observations[11];size_t count=99;
    unsigned before=m.reads;assert(!pd_cio_tunables_observe(&b,&io,observations,11,&count));
    assert(count==11 && m.reads==before+11 && m.writes==11);
    for(const auto &o:observations)assert(o.value==0x80 && m.regs.at({o.window,o.offset})==o.value);
    before=m.reads;
    assert(pd_cio_tunables_observe(&b,&io,observations,10,&count)==EOVERFLOW && !count && m.reads==before);
    assert(pd_cio_tunables_observe(&b,&io,observations,513,&count)==EINVAL && !count && m.reads==before);
    auto savedLast=b.entries[10];b.entries[10].raw=nullptr;
    assert(pd_cio_tunables_observe(&b,&io,observations,11,&count)==ENOENT && !count && m.reads==before);
    b.entries[10]=savedLast;
    unsigned refused=0;
    for(unsigned n=0;n<11;++n) {
        auto saved=b.entries[n];b.entries[n].raw=nullptr;
        assert(pd_cio_tunables_apply(&b,&io,PD_CIO_RC)==ENOENT && m.writes==11);++refused;b.entries[n]=saved;
        for(size_t bytes:{0u,12u,23u,25u}) {
            b.entries[n].bytes=bytes;assert(pd_cio_tunables_apply(&b,&io,PD_CIO_NHI)!=0 && m.writes==11);++refused;
        }
        b.entries[n]=saved;
        auto &v=m.props.at(m.names[n]);auto original=v;
        for(unsigned field:{0u,4u,12u,20u}) {
            v=original;v[field]=field==0?1:field==4?8:1;
            assert(pd_cio_tunables_apply(&b,&io,PD_CIO_PCIE_ADAPTER)==EINVAL && m.writes==11);++refused;
        }
        v=original;v[1]=0x40;
        assert(pd_cio_tunables_apply(&b,&io,PD_CIO_RC)==EINVAL && m.writes==11);++refused;v=original;
    }
    assert(!pd_cio_tunables_apply(&b,&io,PD_CIO_RC) && m.writes==11);
    printf("ACIO 24-byte runtime tunables: eleven group/base mappings, clear/OR-set semantics, skip unchanged and %u late missing/malformed refusals before MMIO pass\n",refused);
}
