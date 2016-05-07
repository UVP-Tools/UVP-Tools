#include <linux/module.h>
#include <linux/netdevice.h>
#include <xen/xenbus.h>

#include "netvmdq.h"

static vmdq_drv_hook_t vmdq_drv_suspendresume_ctls[VMDQ_DRV_HOOK_MAX];

/**
 *  vmdq_register_drv_suspendresume - register vnic suspend and resume.
 *  @drv_hook: vnic suspend function and resume function 
 *
 *  The function is used for registering vnic suspend and resume for vmdq 
 *  migration .
 *  unregister function should be called in appropriate place.
 *  
 *  
 *  return 0 register successful 
 *  return -EINVAL  register failed
 */
int vmdq_register_drv_suspendresume( vmdq_drv_hook_t *drv_hook)
{
    vmdq_drv_hook_t *drv = NULL;
    int i = 0;

    if( NULL == drv_hook){
        printk(KERN_WARNING "%s  drv_hook is null !\n", __func__);
        return -EINVAL;
    }

    for(i = 0; i < VMDQ_DRV_HOOK_MAX; i++){
        /*if the register is empty,registe it*/ 
        if(NULL == vmdq_drv_suspendresume_ctls[i].name){
            drv = &vmdq_drv_suspendresume_ctls[i];
            break;
        }
    }

    if(NULL == drv){
        printk(KERN_WARNING "%s register is full !\n", __func__);
        return -EINVAL;
    }

    /*register suspend and resume function*/
    memmove(drv, drv_hook, sizeof(vmdq_drv_hook_t));

    printk(KERN_INFO "%s register successful !\n", __func__);

    return 0;
}
EXPORT_SYMBOL(vmdq_register_drv_suspendresume);

/**
 *  vmdq_unregister_drv_suspendresume - unregister vnic suspend and resume.
 *  @drv_hook: vnic suspend function and resume function 
 *
 *  the function is used for unregistering vnic suspend and resume.
 *  
 *  return 0 unregister successful 
 *  return -EINVAL  unregister failed
 */
int vmdq_unregister_drv_suspendresume(vmdq_drv_hook_t *drv_hook)
{
    int i = 0;

    if( NULL == drv_hook){
        printk(KERN_WARNING "%s drv_hook is null!\n", __func__);
        return -EINVAL;
    }

    for(i = 0; i < VMDQ_DRV_HOOK_MAX; i++){
        if(vmdq_drv_suspendresume_ctls[i].name != drv_hook->name){
            continue;
        }

        /*reset function*/
        memset(&vmdq_drv_suspendresume_ctls[i], 0, sizeof(vmdq_drv_hook_t));

        printk("%s  unregister successful !\n", __func__);
        return 0;
    }

    printk(KERN_ERR "%s unregister failed!\n", __func__);

    return -EINVAL;
} 
EXPORT_SYMBOL(vmdq_unregister_drv_suspendresume);
/**
 *  vmdqfront_probe - probe.
 *  @dev: xenbus subdevice
 *
 *  NULL.
 *  
 *  always return 0 
 *  
 */
static int __devinit vmdqfront_probe(struct xenbus_device *dev,
				    const struct xenbus_device_id *id)
{	
    return 0;
}

/* read vmdq_vnic mac from xenstore*/
static int vmdq_read_mac(struct xenbus_device *dev, u8 mac[])
{
    char *s = NULL;
	char *macstr = NULL;
	char *e = NULL;
	int i = 0;
	
	macstr = s = xenbus_read(XBT_NIL, dev->nodename ,"mac",NULL);
	if(NULL == macstr){
	    return -1;
	}
	
	for(i = 0; i < ETH_ALEN; i++){
	    mac[i] = simple_strtoul(s, &e, 16);       
	    if ((s == e) || (*e != ((i == ETH_ALEN-1) ? '\0' : ':'))) {
	        kfree(macstr);
	        return -1;
	    }
        
        s = e + 1;		
	}
	
	kfree(macstr);
	return 0;
}
/**
 *  vmdqfront_suspend - suspend vmdq_vnic.
 *  @dev: xenbus subdevice
 *
 *  the function is call back vnic register suspend function.
 *  
 *  return 0 suspend successful 
 *  
 */
static int vmdqfront_suspend(struct xenbus_device *dev)
{
    int i = 0;
    int ret = 0;
    unsigned char vmdq_mac[ETH_ALEN] = {0};

    printk("%s\n", dev->nodename);
	
    ret = vmdq_read_mac(dev,vmdq_mac);
    if(ret){
        printk("read vmdq mac failed!\n");
        return -1;
    }
	
    for(i = 0; i < VMDQ_DRV_HOOK_MAX; i++){
        if(vmdq_drv_suspendresume_ctls[i].name != NULL){
            ret = memcmp(vmdq_mac,vmdq_drv_suspendresume_ctls[i].mac,ETH_ALEN);
            if(0 == ret){
                break;
            }		
        }
    }
	
    if( VMDQ_DRV_HOOK_MAX == i){
        printk("vmdq mac compare failed!\n");
        return -1;
    }
	
    /*call back vnic registe suspend function*/
    ret = vmdq_drv_suspendresume_ctls[i].suspend(vmdq_drv_suspendresume_ctls[i].name);
    if(ret){
        printk("registe inic %d suspend failed\n", i );
    }

    return 0;
}

/**
 *  vmdqfront_resume - resume vmdq_vnic.
 *  @dev: xenbus subdevice
 *
 *  the function is call back vnic registe  resume function.
 *  
 *  return 0 resume successful 
 *  
 */
static int vmdqfront_resume(struct xenbus_device *dev)
{
    int i = 0;
    int ret = 0;
    unsigned char vmdq_mac[ETH_ALEN] = {0};

    printk("%s\n", dev->nodename);
	
    ret = vmdq_read_mac(dev,vmdq_mac);
    if(ret){
        printk("read vmdq mac failed!\n");
        return -1;
    }
	
    for(i = 0; i < VMDQ_DRV_HOOK_MAX; i++){
        if(vmdq_drv_suspendresume_ctls[i].name != NULL){
            ret = memcmp(vmdq_mac,vmdq_drv_suspendresume_ctls[i].mac,ETH_ALEN);
            if(0 == ret){
                break;
            }
        }
    }
    
    if( VMDQ_DRV_HOOK_MAX == i){
        printk("vmdq mac compare failed or vmdq driver error, ignore this!\n");
        return 0;
    }

    /*call back vnic registe resume function*/
    ret = vmdq_drv_suspendresume_ctls[i].resume(vmdq_drv_suspendresume_ctls[i].name);
    if(ret){
        printk("registe inic %d resume failed\n", i );
    }

    return 0;
}


/* ** Driver registration ** */

static const struct xenbus_device_id vmdqfront_ids[] = {
	{ "vmdq_vnic" },
	{ "" }
};
MODULE_ALIAS("xen:vmdq_vnic");

#if (defined SUSE_1103)
static DEFINE_XENBUS_DRIVER(vmdqfront, ,
	.probe = vmdqfront_probe,
	.suspend = vmdqfront_suspend,
	.suspend_cancel = vmdqfront_resume,
	.resume = vmdqfront_resume,
);
#else
static struct xenbus_driver vmdqfront_driver = {
	.name = "vmdq_vnic",
	.ids = vmdqfront_ids,
	.probe = vmdqfront_probe,
	.suspend = vmdqfront_suspend,
	.suspend_cancel = vmdqfront_resume,
	.resume = vmdqfront_resume,
};
#endif

static int __init netvmdq_init(void)
{
    int err;

    err = xenbus_register_frontend(&vmdqfront_driver);
    if (err) {
        printk(KERN_ERR "vmdqfront_driver register failed!\n");
    }
    
    return err;
}
module_init(netvmdq_init);

static void __exit netvmdq_exit(void)
{
    xenbus_unregister_driver(&vmdqfront_driver);
}
module_exit(netvmdq_exit);

MODULE_LICENSE("Dual BSD/GPL");
