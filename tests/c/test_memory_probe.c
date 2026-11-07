#include "memory_probe.h"
#include "mapping_ticket.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define OK(x) assert((x)==DL_OK)
#define IS(x,v) assert((x)==(v))
static unsigned tests;
typedef struct { void *bytes; bool exported; } block;
typedef struct {
    unsigned allocs,frees,live,enumerations,exports;
    bool fail_alloc,fail_pages,fail_release,quarantine,reenter;
    dl_probe *probe;
} host;
typedef struct { host h; dl_probe *p; void *storage; uint64_t control,owner; } fixture;
static void *allocate(void *arg,uint64_t bytes,uint64_t align) {
    host *h=arg; ++h->allocs;
    if(h->fail_alloc) return NULL;
    block *b=calloc(1,sizeof *b); assert(b);
    b->bytes=aligned_alloc((size_t)align,(size_t)bytes); assert(b->bytes);
    memset(b->bytes,0,(size_t)bytes); ++h->live;
    if(h->reenter) { uint64_t id=0; IS(dl_probe_open(h->probe,&id),DL_BAD_STATE); }
    return b;
}
static bool release(void *arg,void *cookie,uint64_t bytes) {
    host *h=arg; block *b=cookie; (void)bytes;
    if(h->fail_release || (h->quarantine && b->exported)) return false;
    free(b->bytes); free(b); ++h->frees; --h->live; return true;
}
static int pages(void *arg,void *cookie,uint64_t offset,uint64_t bytes,
                 uint64_t page,uint32_t *out,size_t count) {
    host *h=arg; (void)cookie;(void)offset; ++h->enumerations;
    if(h->reenter) { uint64_t id=0; IS(dl_probe_open(h->probe,&id),DL_BAD_STATE); }
    if(h->fail_pages) return DL_STORE_BACKEND_ERROR;
    assert(count==bytes/page);
    /* Synthetic page numbers only. No claim of physical host addresses. */
    for(size_t i=0;i<count;i++) out[i]=(uint32_t)(4096+i);
    return DL_OK;
}
static void exported(void *arg,void *cookie) {
    host *h=arg; ((block *)cookie)->exported=true; ++h->exports;
}
static void setup(fixture *f,uint64_t page,uint64_t budget) {
    *f=(fixture){0};f->storage=calloc(1,dl_probe_size());assert(f->storage);
    dl_probe_ops ops={{allocate,release},pages,exported,DLMP_CAP_RAM_ONLY|DLMP_CAP_MAPPED_RECLAIM};
    OK(dl_probe_init(f->storage,dl_probe_size(),page,budget,ops,&f->h,&f->p));
    f->h.probe=f->p; OK(dl_probe_open(f->p,&f->control)); OK(dl_probe_open(f->p,&f->owner));
}
static struct dlmp_alloc alloc_for(fixture *f,uint64_t id,uint64_t bytes) {
    struct dlmp_alloc a={.version=DLMP_VERSION,.bytes=bytes};
    OK(dl_probe_command(f->p,id,DLMP_OP_ALLOC,&a,sizeof a));return a;
}
static struct dlmp_collect collect(fixture *f) {
    struct dlmp_collect c={.version=DLMP_VERSION};
    OK(dl_probe_command(f->p,f->control,DLMP_OP_COLLECT,&c,sizeof c));return c;
}
static struct dlmp_stats stats(fixture *f) {
    struct dlmp_stats s={.version=DLMP_VERSION};
    OK(dl_probe_command(f->p,f->control,DLMP_OP_STATS,&s,sizeof s));return s;
}
static void finish(fixture *f,const char *label) {
    if(f->owner) OK(dl_probe_close(f->p,f->owner));
    collect(f);assert(!f->h.live);OK(dl_probe_close(f->p,f->control));
    free(f->storage);printf("ok %u - %s\n",++tests,label);
}
static void basic(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);
    struct dlmp_info i={.version=DLMP_VERSION};
    OK(dl_probe_command(f.p,f.control,DLMP_OP_INFO,&i,sizeof i));
    assert(i.page_size==16384 && i.max_allocation==DLMP_MAX_ALLOCATION);
    assert(!(i.capabilities&DLMP_CAP_KERNEL_BACKEND));
    struct dlmp_alloc a=alloc_for(&f,f.owner,1);assert(a.bytes==16384 && a.object_id);
    assert(stats(&f).resident_bytes==16384);
    finish(&f,"allocation rounding, typed info and real host backing");
}
static void handoff(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);alloc_for(&f,f.owner,32768);
    uint32_t p[2]; void (*fn)(void *)=NULL; void *arg=NULL;
    OK(dl_probe_map(f.p,f.owner,0,32768,3,p,2,&fn,&arg));assert(fn&&arg&&f.h.exports==1);
    OK(dl_probe_close(f.p,f.owner));f.owner=0;
    struct dlmp_collect c=collect(&f);assert(c.retained==1 && !c.reclaimed);
    fn(arg);fn(arg);assert(!f.h.frees);c=collect(&f);
    assert(c.drained==1 && c.reclaimed==1 && f.h.frees==1);
    fn(arg);c=collect(&f);assert(!c.drained && !c.reclaimed);
    finish(&f,"close retains mapped memory and duplicate notifications drain once");
}
static void multiple_maps(void) {
    fixture f;setup(&f,4096,DLMP_BUDGET);alloc_for(&f,f.owner,8192);
    uint32_t p;void (*fa)(void *),(*fb)(void *);void *aa,*ab;
    OK(dl_probe_map(f.p,f.owner,0,4096,1,&p,1,&fa,&aa));
    OK(dl_probe_map(f.p,f.owner,4096,4096,3,&p,1,&fb,&ab));
    OK(dl_probe_close(f.p,f.owner));f.owner=0; fa(aa);
    assert(collect(&f).retained==1);assert(stats(&f).pending_maps==1);
    fb(ab);assert(collect(&f).reclaimed==1);
    finish(&f,"independent mappings retain shared allocation until both callbacks");
}
static void malformed(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);
    struct dlmp_info i={.version=99};
    IS(dl_probe_command(f.p,f.owner,DLMP_OP_INFO,&i,sizeof i),DL_INVALID);
    i.version=1;i.flags=1;IS(dl_probe_command(f.p,f.owner,DLMP_OP_INFO,&i,sizeof i),DL_INVALID);
    i.flags=0;IS(dl_probe_command(f.p,f.owner,DLMP_OP_INFO,&i,sizeof i-1),DL_INVALID);
    IS(dl_probe_command(f.p,f.owner,99,&i,sizeof i),DL_INVALID);
    IS(dl_probe_command(f.p,999,DLMP_OP_INFO,&i,sizeof i),DL_STALE);
    struct dlmp_alloc a={.version=1,.bytes=1,.reserved={1,0}};
    IS(dl_probe_command(f.p,f.owner,DLMP_OP_ALLOC,&a,sizeof a),DL_INVALID);
    a.reserved[0]=0;a.object_id=1;
    IS(dl_probe_command(f.p,f.owner,DLMP_OP_ALLOC,&a,sizeof a),DL_INVALID);
    a.object_id=0;a.bytes=UINT64_MAX;
    IS(dl_probe_command(f.p,f.owner,DLMP_OP_ALLOC,&a,sizeof a),DL_INVALID);
    a.bytes=0;IS(dl_probe_command(f.p,f.owner,DLMP_OP_ALLOC,&a,sizeof a),DL_INVALID);
    assert(!f.h.allocs);finish(&f,"version, reserved fields, command sizes and allocation limits");
}
static void invalid_maps(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);alloc_for(&f,f.owner,32768);
    uint32_t p[2];void (*fn)(void *)=NULL;void *arg=NULL;
    IS(dl_probe_map(f.p,f.owner,1,16384,1,p,1,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.owner,0,16383,1,p,1,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.owner,32768,16384,1,p,1,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.owner,UINT64_MAX-16383,16384,1,p,1,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.owner,0,16384,4,p,1,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.owner,0,16384,2,p,1,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.owner,0,16384,1,p,2,&fn,&arg),DL_INVALID);
    IS(dl_probe_map(f.p,f.control,0,16384,1,p,1,&fn,&arg),DL_INVALID);
    assert(!fn&&!arg&&!f.h.enumerations);finish(&f,"mapping bounds and permissions rejected before page provider");
}
static void failed_pages(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);alloc_for(&f,f.owner,16384);f.h.fail_pages=true;
    uint32_t p;void (*fn)(void *)=NULL;void *arg=NULL;
    IS(dl_probe_map(f.p,f.owner,0,16384,1,&p,1,&fn,&arg),DL_STORE_BACKEND_ERROR);
    assert(!fn&&!arg&&!f.h.exports);assert(!stats(&f).published_maps);
    finish(&f,"page-enumeration failure aborts hold without publishing callback");
}
static void quarantine(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);alloc_for(&f,f.owner,16384);f.h.quarantine=true;
    uint32_t p;void (*fn)(void *);void *arg;
    OK(dl_probe_map(f.p,f.owner,0,16384,1,&p,1,&fn,&arg));
    OK(dl_probe_close(f.p,f.owner));f.owner=0;fn(arg);
    struct dlmp_collect c=collect(&f);
    assert(c.backend_failures==1&&c.retained==1&&!c.reclaimed&&stats(&f).resident_bytes==16384);
    assert(collect(&f).backend_failures==1);
    /* Host-only cleanup after the synthetic callback producer has returned. */
    f.h.quarantine=false;assert(collect(&f).reclaimed==1);
    finish(&f,"kernel-style quarantine reports blocked reclamation rather than a pass");
}
static void failed_allocation(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);f.h.fail_alloc=true;
    struct dlmp_alloc a={.version=1,.bytes=1};
    IS(dl_probe_command(f.p,f.owner,DLMP_OP_ALLOC,&a,sizeof a),DL_STORE_BACKEND_ERROR);
    assert(a.bytes==1&&!a.object_id&&!stats(&f).resident_bytes);
    f.h.fail_alloc=false;alloc_for(&f,f.owner,1);finish(&f,"allocation failure publishes no object and remains retryable");
}
static void failed_release(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);alloc_for(&f,f.owner,1);
    OK(dl_probe_close(f.p,f.owner));f.owner=0;f.h.fail_release=true;
    assert(collect(&f).backend_failures==1&&f.h.live==1);
    f.h.fail_release=false;assert(collect(&f).reclaimed==1);
    finish(&f,"release errors remain visible and preserve backing ownership");
}
static void stale(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);uint64_t old=f.owner;
    OK(dl_probe_close(f.p,f.owner));OK(dl_probe_open(f.p,&f.owner));assert(old!=f.owner);
    IS(dl_probe_close(f.p,old),DL_STALE);
    struct dlmp_info i={.version=1};IS(dl_probe_command(f.p,old,DLMP_OP_INFO,&i,sizeof i),DL_STALE);
    finish(&f,"reused session slots reject stale identities");
}
static void capacity(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);uint64_t ids[DLMP_SESSION_CAPACITY-2],out=99;
    for(unsigned i=0;i<DLMP_SESSION_CAPACITY-2;i++) OK(dl_probe_open(f.p,&ids[i]));
    IS(dl_probe_open(f.p,&out),DL_CAPACITY);assert(out==99);
    for(unsigned i=0;i<DLMP_SESSION_CAPACITY-2;i++) OK(dl_probe_close(f.p,ids[i]));
    finish(&f,"bounded sessions exhaust without publishing an invalid handle");
}
static void ticket_capacity(void) {
    fixture f;setup(&f,4096,DLMP_BUDGET);alloc_for(&f,f.owner,4096);
    uint32_t p;void (*fn)(void *);void *arg;
    for(unsigned i=0;i<DLMP_TICKET_CAPACITY;i++) {
        OK(dl_probe_map(f.p,f.owner,0,4096,1,&p,1,&fn,&arg));fn(arg);
        assert(collect(&f).drained==1);
    }
    IS(dl_probe_map(f.p,f.owner,0,4096,1,&p,1,&fn,&arg),DL_CAPACITY);
    finish(&f,"single-use tickets exhaust deliberately instead of unsafe reuse");
}
static void budget(void) {
    fixture f;setup(&f,16384,16384);alloc_for(&f,f.owner,1);
    struct dlmp_alloc a={.version=1,.bytes=1};
    IS(dl_probe_command(f.p,f.control,DLMP_OP_ALLOC,&a,sizeof a),DL_CAPACITY);
    assert(f.h.allocs==1);finish(&f,"budget rejects allocations before provider side effects");
}
static void reentry(void) {
    fixture f;setup(&f,16384,DLMP_BUDGET);f.h.reenter=true;alloc_for(&f,f.owner,1);
    uint32_t p;void (*fn)(void *);void *arg;
    OK(dl_probe_map(f.p,f.owner,0,16384,1,&p,1,&fn,&arg));fn(arg);collect(&f);
    finish(&f,"allocation and page callbacks cannot reenter service metadata");
}
static int visitor(void *arg,void *cookie,uint64_t bytes) {
    dl_store *s=arg;assert(cookie&&bytes==4096);dl_buffer b;
    IS(dl_store_allocate(s,1,&b),DL_BAD_STATE);return 123;
}
static void borrowing(void) {
    host h={0};dl_store s;OK(dl_store_init(&s,1,4096,4096,(dl_backing_ops){allocate,release},&h));
    dl_buffer b;OK(dl_store_allocate(&s,1,&b));
    IS(dl_store_visit_backing(&s,b,visitor,&s),DL_BUSY);
    dl_lease l;OK(dl_store_hold(&s,b,DL_CPU_MAP,&l));
    IS(dl_store_visit_backing(&s,b,visitor,&s),123);
    OK(dl_store_release_host(&s,l));OK(dl_store_close_owner(&s,b));dl_collection c;
    OK(dl_store_collect(&s,&c));IS(dl_store_visit_backing(&s,b,visitor,&s),DL_STALE);
    assert(!h.live);printf("ok %u - retained-cookie visitor rejects unheld and stale backing\n",++tests);
}
static void initialization(void) {
    void *p=calloc(1,dl_probe_size());assert(p);dl_probe *out=NULL;
    dl_probe_ops ops={{allocate,release},pages,exported,DLMP_CAP_RAM_ONLY};
    IS(dl_probe_init(p,dl_probe_size(),3,4096,ops,NULL,&out),DL_INVALID);
    IS(dl_probe_init(p,dl_probe_size()-1,4096,4096,ops,NULL,&out),DL_INVALID);
    OK(dl_probe_init(p,dl_probe_size(),4096,4096,ops,NULL,&out));
    IS(dl_probe_init(p,dl_probe_size(),4096,4096,ops,NULL,&out),DL_BAD_STATE);
    free(p);printf("ok %u - initialization validates storage, granule and reinitialization\n",++tests);
}
int main(void) {
    basic();handoff();multiple_maps();malformed();invalid_maps();failed_pages();quarantine();
    failed_allocation();failed_release();stale();capacity();ticket_capacity();budget();reentry();borrowing();initialization();
    printf("1..%u\n",tests);return 0;
}
