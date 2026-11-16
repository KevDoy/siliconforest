/* SPDX-License-Identifier: MIT */
/* Literal patched decode function, with deterministic fake hardware providers.
 * Exercises counter placement/outcomes; does not execute a decoder or IRQ. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <pdavd/pdavd_ioctl.h>
typedef uint64_t u64;
typedef uint8_t u8;
@STATS@
enum vb2_buffer_state { VB2_BUF_STATE_ERROR, VB2_BUF_STATE_DONE };
enum { AVD_JOB_RUNNING, AVD_JOB_SUBMITTED, AVD_JOB_FINISHING };
enum { PD_AVD_DMA_BEFORE_DEVICE=1, PD_AVD_DMA_AFTER_DEVICE, PD_AVD_DMA_PUBLISH_CPU };
#define V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF 1
#define AVD_REG_MBOX_IRQ_ENABLE 0
#define spin_lock_irqsave(lock, flags) do { (void)(lock); (flags)=0; } while (0)
#define spin_unlock_irqrestore(lock, flags) do { (void)(lock); (void)(flags); } while (0)
#define dev_err(...) ((void)0)
struct pd_avd_session;
struct avd_ctx;
struct ops { int (*run)(struct avd_ctx *); };
struct avd_coded_fmt_desc { struct ops *ops; };
struct avd_ctx { const struct avd_coded_fmt_desc *coded_fmt_desc; struct pd_avd_session *session; };
struct plane { unsigned length,bytesused; };
struct buffer { u64 timestamp; bool copied_timestamp; struct plane planes[1]; void *pd_vaddr; u64 pd_dma; };
struct vb { struct buffer vb2_buf; };
struct source { struct buffer vb2_buf; struct plane planes[1]; unsigned flags; };
struct avd_decoded_buffer { struct { struct vb vb; } base; unsigned grey_chroma_offset; };
struct avd_dev {
 int job_lock,job_state; u64 job_seq; struct avd_ctx *job_ctx;
 bool job_end_sent,job_pending; enum vb2_buffer_state job_pending_result;
 void *dev; unsigned *mbox;
};
struct pd_avd_device {
 struct avd_dev avd; struct pd_avd_stats stats;
 bool booted,done; enum vb2_buffer_state done_state; struct pd_avd_session *current;
};
struct pd_avd_session {
 struct pd_avd_device *dev; struct avd_ctx ctx; struct pd_avd_stats stats;
 bool started,imported,submitted; unsigned ncap;
 struct { void *cpu; size_t size; u64 addr; } coded;
 struct source src;
 struct { struct vb *pd_dst; bool pd_finished; enum vb2_buffer_state pd_result; } m2m;
 struct avd_decoded_buffer *cap[1];
};
enum mode { SUCCESS,HOLD,RUN_ERROR,BAD_COMPLETION,TIMEOUT,AFTER_ERROR,GREY_ERROR,BEFORE_ERROR };
static enum mode mode;
static u64 now;
static int sync_calls;
static uint64_t pd_avd_os_now_us(void) { now+=100; return now; }
static int pd_avd_session_buffer(struct pd_avd_session *s,unsigned i,void **cpu,size_t *bytes,void **cookie) {
 (void)s; assert(!i); *cpu=NULL; *bytes=16384; *cookie=(void *)(uintptr_t)1; return 0;
}
static int pd_avd_os_dma_sync(void *cookie,int direction) {
 assert(cookie); sync_calls++;
 return ((mode==BEFORE_ERROR&&direction==PD_AVD_DMA_BEFORE_DEVICE)||
         (mode==AFTER_ERROR&&direction==PD_AVD_DMA_AFTER_DEVICE)||
         (mode==GREY_ERROR&&direction==PD_AVD_DMA_PUBLISH_CPU))?-EIO:0;
}
static void avd_job_finish(struct avd_ctx *ctx,enum vb2_buffer_state result) {
 ctx->session->m2m.pd_finished=true; ctx->session->m2m.pd_result=result;
}
static int run(struct avd_ctx *ctx) {
 struct pd_avd_session *s=ctx->session;
 if(mode==RUN_ERROR)return -EIO;
 if(mode==HOLD){avd_job_finish(ctx,VB2_BUF_STATE_DONE);s->dev->avd.job_state=AVD_JOB_FINISHING;}
 if(mode==GREY_ERROR)s->cap[0]->grey_chroma_offset=1;
 return 0;
}
static bool pd_avd_os_job_wait(struct pd_avd_device *dev,uint32_t timeout) {
 assert(timeout==2000);
 if(mode==TIMEOUT)return false;
 dev->done=true;dev->done_state=mode==BAD_COMPLETION?VB2_BUF_STATE_ERROR:VB2_BUF_STATE_DONE;
 return true;
}
static void avd_status(struct avd_dev *avd,int index){(void)avd;(void)index;}
static void writel(unsigned value,unsigned *where){*where=value;}
static int avd_reset(struct pd_avd_device *dev){(void)dev;return 0;}
static void grey_chroma(struct pd_avd_session *s,struct avd_decoded_buffer *buf){(void)s;(void)buf;}
@DECODE@
@SESSION_GETTER@
static void init(struct pd_avd_session *s,struct pd_avd_device *dev,struct avd_decoded_buffer *cap) {
 static struct ops ops={run};static struct avd_coded_fmt_desc desc={&ops};
 memset(s,0,sizeof *s);s->dev=dev;s->ctx.coded_fmt_desc=&desc;s->ctx.session=s;
 s->started=true;s->imported=true;s->ncap=1;s->coded.cpu=s;s->coded.size=16384;s->cap[0]=cap;
}
static struct pd_avd_stats scenario(enum mode selected,int expected) {
 unsigned mmio=123;struct pd_avd_device dev={0};struct avd_decoded_buffer cap={0};struct pd_avd_session s;
 dev.booted=true;dev.avd.mbox=&mmio;init(&s,&dev,&cap);mode=selected;now=0;sync_calls=0;
 uint32_t status=99;assert(pd_avd_session_decode(&s,10,0,123,selected==HOLD?PDAVD_DECODE_HOLD:0,&status)==expected);
 struct pd_avd_stats stats;pd_avd_session_stats(&s,&stats);
 assert(stats.jobs==dev.stats.jobs&&stats.done==dev.stats.done&&stats.errors==dev.stats.errors&&stats.timeouts==dev.stats.timeouts);
 assert(stats.last_decode_us==dev.stats.last_decode_us&&stats.total_decode_us==dev.stats.total_decode_us&&stats.max_decode_us==dev.stats.max_decode_us);
 if(selected==BEFORE_ERROR)assert(!s.submitted&&sync_calls==1);else assert(s.submitted);
 return stats;
}
int main(void) {
 struct pd_avd_stats stats=scenario(SUCCESS,0);assert(stats.jobs==1&&stats.done==1&&stats.total_decode_us==100&&!stats.errors&&!stats.timeouts);
 stats=scenario(HOLD,0);assert(stats.jobs==1&&!stats.done&&!stats.total_decode_us);
 stats=scenario(RUN_ERROR,-EIO);assert(stats.jobs==1&&stats.errors==1&&!stats.done);
 stats=scenario(BAD_COMPLETION,-EIO);assert(stats.jobs==1&&stats.errors==1&&!stats.done);
 stats=scenario(TIMEOUT,-ETIMEDOUT);assert(stats.jobs==1&&stats.timeouts==1&&!stats.errors&&!stats.done);
 stats=scenario(BEFORE_ERROR,-EIO);assert(!stats.jobs&&!stats.errors&&!stats.done);
 stats=scenario(AFTER_ERROR,-EIO);assert(stats.jobs==1&&!stats.errors&&!stats.done);
 stats=scenario(GREY_ERROR,-EIO);assert(stats.jobs==1&&!stats.errors&&!stats.done);
 unsigned mmio=0;struct pd_avd_device dev={0};struct avd_decoded_buffer caps[2]={{0},{0}};struct pd_avd_session a,b;
 dev.booted=true;dev.avd.mbox=&mmio;init(&a,&dev,&caps[0]);init(&b,&dev,&caps[1]);
 mode=SUCCESS;assert(pd_avd_session_decode(&a,0,0,0,0,NULL)==-EINVAL);assert(!a.stats.jobs);
 for(unsigned i=0;i<100;i++)assert(pd_avd_session_decode(i%3?&a:&b,10,0,0,0,NULL)==0);
 assert(a.stats.jobs==66&&a.stats.done==66&&b.stats.jobs==34&&b.stats.done==34&&dev.stats.jobs==100);
 puts("PASS actual decode branches: success/HOLD/run-error/bad-completion/timeout/pre-sync/post-sync/grey-sync/admission + two-session interleaving");
 puts("LIMIT: fake hardware/run/wait providers; no native job, DMA, interrupt, lock or timeout recovery evidence");
}
