// SPDX-License-Identifier: MIT
#include "pd_cio_lifecycle.h"
#include <vector>
#include <cassert>
#include <cerrno>
#include <cstdio>
struct Model {
    std::vector<unsigned> trace;
    unsigned fail=0, teardownFail=0;
    bool power=false,fw=false,children=false;
    int step(unsigned n) {
        trace.push_back(n);
        if(n>=4 && n<=10)assert(power);
        if(n>=8 && n<=10)assert(fw);
        if(n==3)power=true;
        if(n==5)fw=true;
        if(n==8)children=true;
        if(n==11){assert(power&&fw&&children);if(teardownFail==n)return -EBUSY;children=false;}
        if(n==12){assert(power&&fw&&!children);if(teardownFail==n)return -EBUSY;fw=false;}
        if(n==13){assert(power&&!fw&&!children);if(teardownFail==n)return -EBUSY;power=false;}
        return n==fail?-EIO:0;
    }
    static int phy(void *c,uint32_t info){assert(info&1);return ((Model*)c)->step(1);}
    static int cable(void *c,uint32_t info){assert(info&1);auto &m=*(Model*)c;assert(m.children);return m.step(10);}
#define STEP(name,n) static int name(void *c){return ((Model*)c)->step(n);}
    STEP(pcie,2) STEP(on,3) STEP(reset,4) STEP(rtkit,5) STEP(ready,6) STEP(tunables,7)
    STEP(add,8) STEP(domain,9) STEP(remove,11) STEP(stop,12) STEP(off,13)
    pd_cio_ops ops(){return {this,phy,pcie,on,reset,rtkit,ready,tunables,add,domain,cable,remove,stop,off};}
};
int main(){
    for(unsigned failure=0;failure<=10;++failure) {
        Model m;auto ops=m.ops();pd_cio_session s={};m.fail=failure;
        int ret=pd_cio_start(&s,&ops,0x401);
        assert(ret==(failure?-EIO:0));
        if(!failure) {
            assert(s.ready&&s.generation==1&&s.cable_info==0x401);
            auto count=m.trace.size();assert(!pd_cio_start(&s,&ops,0x401)&&m.trace.size()==count);
            assert(pd_cio_start(&s,&ops,0x403)==-EINVAL); // full stop before new orientation
            assert(!s.ready&&!m.power&&!m.children&&!m.fw);
            assert(!pd_cio_start(&s,&ops,0x403)&&s.generation==2);
            assert(!pd_cio_stop(&s,&ops));
        }
        assert(!s.ready&&!s.power&&!s.rtkit&&!s.children&&!m.power&&!m.fw&&!m.children);
    }
    for(unsigned failure:{11,12,13}) {
        Model m;auto ops=m.ops();pd_cio_session s={};
        assert(!pd_cio_start(&s,&ops,0x401));m.teardownFail=failure;
        assert(pd_cio_stop(&s,&ops)==-EBUSY && s.power && m.power && s.closing && !s.ready);
        assert(pd_cio_start(&s,&ops,0x401)==-EBUSY);
        m.teardownFail=0;assert(!pd_cio_stop(&s,&ops)&&!m.power&&!s.closing);
    }
    Model m;auto ops=m.ops();pd_cio_session s={};m.fail=9;m.teardownFail=11;
    assert(pd_cio_start(&s,&ops,0x401)==-EIO && s.children && s.power && s.closing);
    m.teardownFail=0;assert(!pd_cio_stop(&s,&ops));
    s.generation=UINT64_MAX;
    assert(pd_cio_start(&s,&ops,0x401)==-EOVERFLOW);
    ops.rtkit_start=nullptr;assert(pd_cio_start(&s,&ops,0x401)==-EINVAL);
    puts("ACIO startup failure at every stage; child/RTKit/power drain retention; changed-cable OFF ordering passed");
}
