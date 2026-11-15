/* SPDX-License-Identifier: MIT */
#ifndef PD_TB_CM_HELPERS_H
#define PD_TB_CM_HELPERS_H
#include <linux/random.h>
#include <linux/property.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/unaligned.h>
/* Typed copies preserve the native host byte order without unaligned loads. */
#ifndef get_unaligned
#define get_unaligned(p) ({ __typeof__(*(p)) v;memcpy((void *)&v,(p),sizeof(v));v; })
#define put_unaligned(value,p) do { __typeof__(*(p)) v=(value);memcpy((p),&v,sizeof(v)); } while(0)
#endif
#ifndef get_random_u32
static inline u32 pd_tb_random_u32(void) { u32 v;get_random_bytes(&v,sizeof(v));return v; }
#define get_random_u32 pd_tb_random_u32
#endif
/* The function-like completion macro otherwise also expands cm_ops->complete(). */
#undef complete
static inline void complete(struct completion *c) { linux_complete_common(c,0); }
#undef alloc_ordered_workqueue
static inline struct workqueue_struct *pd_tb_ordered_workqueue(unsigned flags,const char *fmt,...) __attribute__((format(printf,2,3)));
static inline struct workqueue_struct *pd_tb_ordered_workqueue(unsigned flags,const char *fmt,...)
{
 char name[64];va_list args;va_start(args,fmt);int n=vsnprintf(name,sizeof(name),fmt,args);va_end(args);
 if(n<0 || n>=sizeof(name))return NULL;
 /* Both native taskqueue implementations copy name before returning. */
 return flags&WQ_HIGHPRI?linux_create_workqueue_highpri(name,1):linux_create_workqueue_common(name,1);
}
#define alloc_ordered_workqueue(fmt,flags,...) pd_tb_ordered_workqueue((flags),(fmt),##__VA_ARGS__)
static inline int device_property_count_u8(struct device *dev,const char *name)
{ return fwnode_property_read_u8_array(dev_fwnode(dev),name,NULL,0); }
static inline int device_property_read_u8_array(struct device *dev,const char *name,u8 *value,size_t count)
{ return fwnode_property_read_u8_array(dev_fwnode(dev),name,value,count); }
static inline int device_property_read_u8(struct device *dev,const char *name,u8 *value)
{ return device_property_read_u8_array(dev,name,value,1); }
#ifndef SET_RUNTIME_PM_OPS
#define SET_RUNTIME_PM_OPS(s,r,i) .runtime_suspend=(s),.runtime_resume=(r),.runtime_idle=(i),
#endif
#ifndef PCI_EXT_CAP_ID_VNDR
#define PCI_EXT_CAP_ID_VNDR 0x0b
#endif
#endif
