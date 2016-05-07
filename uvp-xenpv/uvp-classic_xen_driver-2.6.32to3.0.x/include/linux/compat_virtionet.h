#ifndef __COMPAT_VIRTIONET_H__
#define __COMPAT_VIRTIONET_H__

#include <linux/version.h>

#ifndef netif_set_real_num_tx_queues
#define netif_set_real_num_tx_queues(dev, max_queue_pairs)  
#endif

#ifndef netif_set_real_num_rx_queues
#define netif_set_real_num_rx_queues(dev, max_queue_pairs) 
#endif

#ifndef this_cpu_ptr
#define this_cpu_ptr(field) \
	per_cpu_ptr(field, smp_processor_id())
#endif

#ifndef net_ratelimited_function
#define net_ratelimited_function(function, ...)			\
do {								\
	if (net_ratelimit())					\
		function(__VA_ARGS__);				\
} while (0)
#endif /* net_ratelimited_function */

#ifndef pr_warn
#define pr_warn(fmt, arg...) printk(KERN_WARNING fmt, ##arg)
#endif /* pr_warn */

#ifndef net_warn_ratelimited
#define net_warn_ratelimited(fmt, ...)				\
	net_ratelimited_function(pr_warn, fmt, ##__VA_ARGS__)
#endif /* net_warn_ratelimited */

#ifndef net_dbg_ratelimited
#define net_dbg_ratelimited(fmt, ...)				\
	net_ratelimited_function(pr_debug, fmt, ##__VA_ARGS__)
#endif /* net_dbg_ratelimited */

#ifndef eth_hw_addr_random
#define eth_hw_addr_random(dev) \
	random_ether_addr(dev->dev_addr)
#endif /* eth_hw_addr_random */

#ifndef netdev_for_each_uc_addr
#define netdev_for_each_uc_addr(ha, dev) \
	list_for_each_entry(ha, &dev->uc.list, list)
#endif /* netdev_for_each_uc_addr */

#ifndef netdev_mc_count
#define netdev_mc_count(dev) \
	dev->mc_count
#endif /* netdev_mc_count */

#ifndef netdev_uc_count
#define netdev_uc_count(dev) \
	dev->uc.count
#endif /* netdev_uc_count */

#ifndef __percpu
#define __percpu
#endif /* __percpu */


#ifndef skb_checksum_start_offset
#define skb_checksum_start_offset(skb) \
	skb->csum_start - skb_headroom(skb)
#endif /* skb_checksum_start_offset */

/**
 * module_driver() - Helper macro for drivers that don't do anything
 * special in module init/exit. This eliminates a lot of boilerplate.
 * Each module may only use this macro once, and calling it replaces
 * module_init() and module_exit().
 *
 * @__driver: driver name
 * @__register: register function for this driver type
 * @__unregister: unregister function for this driver type
 * @...: Additional arguments to be passed to __register and __unregister.
 *
 * Use this macro to construct bus specific macros for registering
 * drivers, and do not use it on its own.
 */
#ifndef module_driver
#define module_driver(__driver, __register, __unregister, ...) \
static int __init __driver##_init(void) \
{ \
	return __register(&(__driver) , ##__VA_ARGS__); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
	__unregister(&(__driver) , ##__VA_ARGS__); \
} \
module_exit(__driver##_exit);
#endif /* module_driver */

#endif /* __COMPAT_VIRTIONET_H__ */