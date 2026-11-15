// SPDX-License-Identifier: MIT
/* Private CM device model. This is metadata/reference management, not a native
 * NHI/PM backend. The controller owner must install real callbacks first. */
#include "pd_tb_device_api.h"
#include "pd_tb_registry.h"
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/errno.h>
static struct pd_tb_native_ops native;
static struct pd_tb_registry registry;
static struct new_utsname os;
static const struct bus_type *registered_bus;
static bool ready;
static int fault;
static bool retain(void *ctx,void *p) { (void)ctx;return kref_get_unless_zero(&((struct device *)p)->kobj.kref); }
static void release(void *ctx,void *p) { (void)ctx;put_device(p); }
bool pd_tb_native_ready(void) { return __atomic_load_n(&ready,__ATOMIC_ACQUIRE) && !__atomic_load_n(&fault,__ATOMIC_ACQUIRE); }
int pd_tb_native_install(const struct pd_tb_native_ops *ops)
{
 if(!ops || !ops->lock || !ops->unlock || !ops->publish || !ops->unpublish || !ops->event || !ops->runtime || !ops->os_info)return -EINVAL;
 if(ready)return -EALREADY;
 struct new_utsname snapshot={0};
 int err=ops->os_info(ops->ctx,&snapshot,sizeof(snapshot));if(err)return err;
 /* All six fixed fields must be terminated; names may legitimately be empty. */
 const char *fields[]={snapshot.sysname,snapshot.nodename,snapshot.release,snapshot.version,snapshot.machine,snapshot.domainname};
 for(unsigned i=0;i<6;i++)if(!memchr(fields[i],0,sizeof(snapshot.sysname)))return -EOVERFLOW;
 struct pd_tb_registry_ops rops={ops->ctx,ops->lock,ops->unlock,retain,release};
 err=pd_tb_registry_init(&registry,&rops);if(err)return err;
 native=*ops;os=snapshot;__atomic_store_n(&ready,true,__ATOMIC_RELEASE);return 0;
}
struct new_utsname *pd_tb_utsname(void) { return pd_tb_native_ready()?&os:NULL; }
int pd_tb_runtime(struct device *dev,enum pd_tb_pm_op op,int argument)
{
 if(!dev || !pd_tb_native_ready())return -ENODEV;
 int err=native.runtime(native.ctx,dev,op,argument);
 if(err<0)__atomic_store_n(&fault,err,__ATOMIC_RELEASE);
 return err;
}
int pd_tb_device_init_wakeup(struct device *dev,bool enabled)
{
 int err=pd_tb_runtime(dev,PD_TB_PM_WAKE_POLICY,enabled);
 if(err>=0){dev->pd_wakeup_enabled=enabled;dev->pd_wakeup_capable=enabled;}
 return err;
}
void pd_tb_device_set_wakeup_capable(struct device *dev,bool capable)
{
 int err=pd_tb_runtime(dev,PD_TB_PM_WAKE_CAPABILITY,capable);
 if(err>=0){dev->pd_wakeup_capable=capable;if(!capable)dev->pd_wakeup_enabled=false;}
}
bool pd_tb_device_may_wakeup(struct device *dev)
{ return dev && pd_tb_native_ready() && dev->pd_wakeup_capable && dev->pd_wakeup_enabled; }
static void final_release(struct kobject *kobj)
{
 struct device *dev=kobj_to_dev(kobj);
 struct device *parent=dev->pd_parent_held?dev->parent:NULL;
 lkpi_devres_release_free_list(dev);
 if(dev->type && dev->type->release)dev->type->release(dev);
 else if(dev->release)dev->release(dev);
 /* The child may already have been freed. No access to dev from here. */
 put_device(parent);
}
static const struct kobj_type native_type={.release=final_release};
void pd_tb_device_initialize(struct device *dev)
{
 /* Upstream domain names its object before initialize. Preserve that name. */
 kobject_init(&dev->kobj,&native_type);
 spin_lock_init(&dev->devres_lock);INIT_LIST_HEAD(&dev->devres_head);INIT_LIST_HEAD(&dev->irqents);
 dev->pd_registered=false;dev->pd_parent_held=false;dev->pd_native_token=NULL;
}
int bus_register(const struct bus_type *bus)
{
 if(!pd_tb_native_ready())return -EPROBE_DEFER;
 if(!bus || !bus->name)return -EINVAL;
 if(registered_bus)return -EBUSY;
 registered_bus=bus;return 0;
}
void bus_unregister(const struct bus_type *bus)
{
 if(bus!=registered_bus)return;
 struct pd_tb_registry_iter it;struct pd_tb_registry_ref ref={0};
 if(pd_tb_registry_begin(&registry,&it,PD_TB_MATCH_BUS,(void *)bus,NULL,false))return;
 int n=pd_tb_registry_next(&registry,&it,&ref);
 if(n!=0){pd_tb_registry_put(&registry,&ref);__atomic_store_n(&fault,-EBUSY,__ATOMIC_RELEASE);return;}
 registered_bus=NULL;
}
/* No optional XDomain service-driver backend yet. Refuse binding explicitly. */
int driver_register(struct device_driver *driver) { (void)driver;return -EOPNOTSUPP; }
void driver_unregister(struct device_driver *driver) { (void)driver; }
int add_uevent_var(struct kobj_uevent_env *env,const char *fmt,...)
{
 if(!env || !fmt || env->count>=ARRAY_SIZE(env->envp) || env->used>=sizeof(env->buf))return -ENOMEM;
 va_list args;va_start(args,fmt);
 int n=vsnprintf(env->buf+env->used,sizeof(env->buf)-env->used,fmt,args);va_end(args);
 if(n<0 || (size_t)n>=sizeof(env->buf)-env->used)return -ENOMEM;
 env->envp[env->count++]=env->buf+env->used;env->used+=(size_t)n+1;return 0;
}
int kobject_uevent(struct kobject *kobj,int action)
{
 if(!kobj || !pd_tb_native_ready())return -ENODEV;
 struct device *dev=kobj_to_dev(kobj);
 if(dev->pd_uevent_suppress)return 0;
 if(!dev->pd_registered || !dev->pd_native_token)return -ENODEV;
 struct kobj_uevent_env env={0};int err=0;
 if(dev->type && dev->type->uevent)err=dev->type->uevent(dev,&env);
 if(!err)err=native.event(native.ctx,dev->pd_native_token,action,env.envp,env.count);
 return err;
}
void dev_set_uevent_suppress(struct device *dev,bool suppress) { dev->pd_uevent_suppress=suppress; }
int pd_tb_device_add(struct device *dev)
{
 if(!dev || !pd_tb_native_ready())return -ENODEV;
 if(dev->pd_registered)return -EEXIST;
 /* Re-adding a deleted object could change its retained parent. Refuse it. */
 if(dev->pd_parent_held)return -EALREADY;
 if(!dev_name(dev))return -EINVAL;
 if(dev->bus) { if(dev->bus!=registered_bus)return -EINVAL; }
 else if(!dev->parent || !dev->parent->pd_registered)return -EINVAL;
 if(dev->kobj.ktype!=&native_type)return -EINVAL;
 if(dev->parent && !retain(NULL,dev->parent))return -ENODEV;
 dev->pd_parent_held=dev->parent!=NULL;
 int err=native.publish(native.ctx,dev,dev->parent,dev_name(dev),dev->type?dev->type->name:NULL,&dev->pd_native_token);
 if(!err && !dev->pd_native_token)err=-EIO;
 if(err)goto fail;
 err=pd_tb_registry_add(&registry,dev,dev->parent,(void *)dev->bus,dev->class);
 if(err)goto fail;
 dev->pd_registered=true;
 err=kobject_uevent(&dev->kobj,KOBJ_ADD);
 if(!err)return 0;
 dev->pd_registered=false;pd_tb_registry_remove(&registry,dev);
fail:
 if(dev->pd_native_token)native.unpublish(native.ctx,dev->pd_native_token);
 dev->pd_native_token=NULL;
 if(dev->pd_parent_held){dev->pd_parent_held=false;put_device(dev->parent);}
 return err;
}
void pd_tb_device_del(struct device *dev)
{
 if(!dev || !dev->pd_registered)return;
 /* The caller owns a reference, as with Linux device_del. */
 int err=kobject_uevent(&dev->kobj,KOBJ_REMOVE);
 if(err)__atomic_store_n(&fault,err,__ATOMIC_RELEASE);
 dev->pd_registered=false;
 native.unpublish(native.ctx,dev->pd_native_token);dev->pd_native_token=NULL;
 pd_tb_registry_remove(&registry,dev);
 /* Keep the child's separate parent reference until final_release. */
}
int pd_tb_device_register(struct device *dev) { pd_tb_device_initialize(dev);return pd_tb_device_add(dev); }
void pd_tb_device_unregister(struct device *dev) { pd_tb_device_del(dev);put_device(dev); }
static int each(enum pd_tb_match match,void *key,struct device *start,bool reverse,void *data,int (*fn)(struct device *,void *))
{
 /* Even a faulted controller must remain traversable for teardown. */
 if(!__atomic_load_n(&ready,__ATOMIC_ACQUIRE))return -ENODEV;
 if(!fn)return -EINVAL;
 struct pd_tb_registry_iter it;int err=pd_tb_registry_begin(&registry,&it,match,key,start,reverse);
 if(err)return err;
 struct pd_tb_registry_ref ref={0};
 while((err=pd_tb_registry_next(&registry,&it,&ref))>0) {
  int result=fn(ref.device,data);pd_tb_registry_put(&registry,&ref);if(result)return result;
 }
 return err;
}
int bus_for_each_dev(const struct bus_type *bus,struct device *start,void *data,int (*fn)(struct device *,void *))
{ return each(PD_TB_MATCH_BUS,(void *)bus,start,false,data,fn); }
int device_for_each_child(struct device *parent,void *data,int (*fn)(struct device *,void *))
{ return each(PD_TB_MATCH_PARENT,parent,NULL,false,data,fn); }
int device_for_each_child_reverse(struct device *parent,void *data,int (*fn)(struct device *,void *))
{ return each(PD_TB_MATCH_PARENT,parent,NULL,true,data,fn); }
struct find_ctx { const void *data;int (*match)(struct device *,const void *);struct device *found; };
static int find_one(struct device *dev,void *arg)
{
 struct find_ctx *f=arg;if(!f->match(dev,f->data))return 0;
 f->found=get_device(dev);return 1;
}
struct device *bus_find_device(const struct bus_type *bus,struct device *start,const void *data,int (*match)(struct device *,const void *))
{
 if(!match)return NULL;struct find_ctx f={data,match,NULL};
 each(PD_TB_MATCH_BUS,(void *)bus,start,false,&f,find_one);return f.found;
}
struct device *device_find_child(struct device *parent,const void *data,int (*match)(struct device *,const void *))
{
 if(!match)return NULL;struct find_ctx f={data,match,NULL};
 each(PD_TB_MATCH_PARENT,parent,NULL,false,&f,find_one);return f.found;
}
