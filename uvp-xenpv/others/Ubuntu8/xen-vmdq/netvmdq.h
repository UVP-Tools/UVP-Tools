#ifndef NETVMDQ_H
#define NETVMDQ_H
#include <linux/kernel.h>

#define VMDQ_DRV_HOOK_MAX  15

typedef struct vmdq_drv_hook{
	void *name;                 //a unique name
	unsigned char mac[ETH_ALEN]; //mac addr
	int(*suspend)(void *name);   //suspend function
	int(*resume)(void *name);    //resume function 
}vmdq_drv_hook_t;

int vmdq_register_drv_suspendresume( vmdq_drv_hook_t *drv_hook);
int vmdq_unregister_drv_suspendresume( vmdq_drv_hook_t *drv_hook);

#endif /*NETVMDQ_H*/
