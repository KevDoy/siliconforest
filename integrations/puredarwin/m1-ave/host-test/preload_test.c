/* SPDX-License-Identifier: MIT */
#include "pdave/pdave_preload.h"
#include "pdave/pdave_identity.h"
#include "pdave/pdave_ioctl.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
struct bank { uint32_t reg[512]; unsigned reads,writes,fail_write; int mismatch; };
static int rd(void *ctx,uint32_t off,uint32_t *out) {
 struct bank *b=ctx;assert(off>=0xdc000 && off<0xdc800 && !(off&3));
 b->reads++;*out=b->reg[(off-0xdc000)/4];
 if(b->mismatch && off==0xdc404)*out^=1;
 return 0;
}
static int wr(void *ctx,uint32_t off,uint32_t value) {
 struct bank *b=ctx;assert(off>=0xdc000 && off<0xdc800 && !(off&3));
 if(++b->writes==b->fail_write)return EIO;
 b->reg[(off-0xdc000)/4]=value;return 0;
}
static void put32(unsigned char *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
int main(void) {
 _Static_assert(sizeof(struct pdave_info)==192,"INFO layout");
 char metadata[48];char padded[256]={0};memcpy(padded,"iBoot-8422.141.2",16);
 assert(!pdave_copy_metadata_text(padded,sizeof padded,metadata,sizeof metadata));
 assert(!strcmp(metadata,"iBoot-8422.141.2"));
 assert(!pdave_copy_metadata_text("version",7,metadata,sizeof metadata)&&!strcmp(metadata,"version"));
 assert(pdave_copy_metadata_text("bad\x01",4,metadata,sizeof metadata)==EINVAL);
 memset(padded,'x',sizeof padded);assert(pdave_copy_metadata_text(padded,sizeof padded,metadata,sizeof metadata)==EINVAL);
 assert(pdave_copy_metadata_text("",1,metadata,sizeof metadata)==EINVAL);
 const char image[]="noise AppleAVE2FW-6070.11.1.release noise RTKit-2062.141.1.release end";
 char version[48];
 assert(!pdave_find_identity(image,sizeof image,"AppleAVE2FW-",version,sizeof version));
 assert(!strcmp(version,"AppleAVE2FW-6070.11.1"));
 assert(pdave_find_identity(image,24,"AppleAVE2FW-",version,sizeof version)==EINVAL);
 const char conflict[]="AppleAVE2FW-1.2\0AppleAVE2FW-3.4\0";
 assert(pdave_find_identity(conflict,sizeof conflict,"AppleAVE2FW-",version,sizeof version)==EINVAL);
 assert(pdave_find_identity(image,sizeof image,"Absent-",version,sizeof version)==ENOENT);
 assert(pdave_find_identity(image,sizeof image,"AppleAVE2FW-",version,8)==EINVAL);
 struct pdave_preload p={.soc=0x8103,.preloaded=1,.segment_count=2,.cold_reserved=1,
  .segment={{0x100000,0,0x100000,0xcc000,3},{0x300000,0xcc000,0x400000,0x128000,0}},
  .iboot="iBoot-8422.141.2",.firmware="AppleAVE2FW-6070.11.1",.rtkit="RTKit-2062.141.1"};
 /* Synthetic addresses and register values only; no captured table/image. */
 assert(!pdave_validate_preload(&p));p.cold_reserved=0;assert(pdave_validate_preload(&p)==ENODEV);p.cold_reserved=1;
 p.firmware="AppleAVE2FW-UNKNOWN";assert(pdave_validate_preload(&p)==ENOTSUP);p.firmware="AppleAVE2FW-6070.11.1";
 p.segment[1].physical=p.segment[0].physical;assert(pdave_validate_preload(&p)==EINVAL);p.segment[1].physical=0x300000;
 p.segment[0].physical=UINT64_MAX-0x3fff;assert(pdave_validate_preload(&p)==EINVAL);
 unsigned char table[504]={0};memcpy(table,"AVEDPE01",8);put32(table+12,1);put32(table+16,39);
 for(unsigned i=0;i<40;i++){put32(table+24+12*i,(i%39+1)*4);put32(table+28+12*i,0xff);put32(table+32+12*i,i%39);}
 struct bank b={0};struct pdave_register_io io={&b,rd,wr};
 for(unsigned n=0;n<sizeof(table);n++)assert(pdave_program_dpe(table,n,&io)==EINVAL);
 assert(!b.reads && !b.writes);
 put32(table+24+39*12,0x400);assert(pdave_program_dpe(table,sizeof(table),&io)==EINVAL);assert(!b.reads&&!b.writes);
 put32(table+24+39*12,4);
 assert(!pdave_program_dpe(table,sizeof(table),&io));assert(b.writes==42);
 assert((b.reg[0xdc400/4-0xdc000/4]&3)==3 && (b.reg[0]&1)==1);
 memset(&b,0,sizeof b);b.fail_write=5;assert(pdave_program_dpe(table,sizeof(table),&io)==EIO);assert(b.writes==5);
 memset(&b,0,sizeof b);b.mismatch=1;assert(pdave_program_dpe(table,sizeof(table),&io)==EIO);
 puts("AVE host preload/DPE: exact ABI/reservation rejection, truncation, late malformed entry before any I/O, masked RMW+enable/readback, fault propagation PASS; synthetic only");
}
