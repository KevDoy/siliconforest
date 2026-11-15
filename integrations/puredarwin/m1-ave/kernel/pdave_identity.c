/* SPDX-License-Identifier: MIT */
#include "pdave/pdave_identity.h"
#ifdef KERNEL
#include <sys/errno.h>
#include <libkern/libkern.h>
#else
#include <errno.h>
#include <string.h>
#endif
int pdave_find_identity(const void *bytes,size_t size,const char *prefix,char *out,size_t capacity) {
 const unsigned char *p=bytes;
 if(!p||!prefix||!out||!capacity)return EINVAL;
 out[0]=0;
 size_t n=strlen(prefix);if(!n||n>=capacity||size>4*1024*1024)return EINVAL;
 int found=0;
 for(size_t i=0;i<size;i++) {
  if(size-i<n || memcmp(p+i,prefix,n))continue;
  size_t end=i+n;
  while(end<size && ((p[end]>='0'&&p[end]<='9') || (p[end]=='.'&&end+1<size&&p[end+1]>='0'&&p[end+1]<='9')))end++;
  if(end==i+n || p[i+n]<'0'||p[i+n]>'9'||p[end-1]=='.')continue;
  size_t count=end-i;
  if(end==size || count>=capacity)return EINVAL;
  if(found && (strlen(out)!=count || memcmp(out,p+i,count)))return EINVAL;
  memcpy(out,p+i,count);out[count]=0;found=1;
 }
 return found?0:ENOENT;
}
