/*
 * Xen SCSI frontend driver
 *
 * Copyright (c) 2008, FUJITSU Limited
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
* Patched to support >2TB drives
* 2010, Samuel Kvasnica, IMS Nanofabrication AG
*/

#include "common.h"
#include <linux/slab.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
  #define DEFAULT_TASK_COMM_LEN	16
#else
  #define DEFAULT_TASK_COMM_LEN	TASK_COMM_LEN
#endif

static unsigned int max_nr_segs = VSCSIIF_SG_TABLESIZE;
module_param_named(max_segs, max_nr_segs, uint, 0);
MODULE_PARM_DESC(max_segs, "Maximum number of segments per request");

extern struct scsi_host_template scsifront_sht;

static int fill_grant_buffer(struct vscsifrnt_info *info, int num)
{
	struct grant *gnt_list_entry, *n;
	struct page *granted_page;
	int i = 0;

	while(i < num) {
		gnt_list_entry = kzalloc(sizeof(struct grant), GFP_NOIO);
		if (!gnt_list_entry)
			goto out_of_memory;

		/* If frontend uses feature_persistent, it will preallocate
		 * (256 + 1) * 256 pages, almost 257M.
		 */
		if (info->feature_persistent) {
			granted_page = alloc_page(GFP_NOIO);
			if (!granted_page) {
				kfree(gnt_list_entry);
				goto out_of_memory;
			}
			gnt_list_entry->pfn = page_to_pfn(granted_page);
		}

		gnt_list_entry->gref = GRANT_INVALID_REF;
		list_add(&gnt_list_entry->node, &info->grants);
		i++;
	}

	return 0;

out_of_memory:
	list_for_each_entry_safe(gnt_list_entry, n,
				             &info->grants, node) {
		list_del(&gnt_list_entry->node);
		if (info->feature_persistent)
			__free_page(pfn_to_page(gnt_list_entry->pfn));
		kfree(gnt_list_entry);
		i--;
	}
	BUG_ON(i != 0);
	return -ENOMEM;
}

static void scsifront_free_ring(struct vscsifrnt_info *info)
{
	int i;

	for (i = 0; i < info->ring_size; i++) {
		if (info->ring_ref[i] != GRANT_INVALID_REF) {
			gnttab_end_foreign_access(info->ring_ref[i], 0UL);
			info->ring_ref[i] = GRANT_INVALID_REF;
		}
	}
	free_pages((unsigned long)info->ring.sring, get_order(VSCSIIF_MAX_RING_PAGE_SIZE));
	info->ring.sring = NULL;

	if (info->irq)
		unbind_from_irqhandler(info->irq, info);
	info->irq = 0;
}

void scsifront_resume_free(struct vscsifrnt_info *info)
{
	struct grant *persistent_gnt;
	struct grant *n;
	int i, j, segs;

	/* Remove all persistent grants */
	if (!list_empty(&info->grants)) {
		list_for_each_entry_safe(persistent_gnt, n,
		                         &info->grants, node) {
			list_del(&persistent_gnt->node);
			if (persistent_gnt->gref != GRANT_INVALID_REF) {
				gnttab_end_foreign_access(persistent_gnt->gref, 0UL);
				info->persistent_gnts_c--;
			}
			if (info->feature_persistent)
				__free_page(pfn_to_page(persistent_gnt->pfn));
			kfree(persistent_gnt);
		}
	}
	BUG_ON(info->persistent_gnts_c != 0);

	/*
	 * Remove indirect pages, this only happens when using indirect
	 * descriptors but not persistent grants
	 */
	if (!list_empty(&info->indirect_pages)) {
		struct page *indirect_page, *n;

		BUG_ON(info->feature_persistent);
		list_for_each_entry_safe(indirect_page, n, &info->indirect_pages, lru) {
			list_del(&indirect_page->lru);
			__free_page(indirect_page);
		}
	}

	for (i = 0; i < RING_SIZE(&info->ring); i++) {
		/*
		 * Clear persistent grants present in requests already
		 * on the shared ring
		 */
		if (!info->shadow[i].sc)
			goto free_shadow;

		segs = info->shadow[i].nr_segments;
		for (j = 0; j < segs; j++) {
			persistent_gnt = info->shadow[i].grants_used[j];
			gnttab_end_foreign_access(persistent_gnt->gref, 0UL);
			if (info->feature_persistent)
				__free_page(pfn_to_page(persistent_gnt->pfn));
			kfree(persistent_gnt);
		}

		if (info->shadow[i].sc_data_direction != DMA_VSCSI_INDIRECT)
			/*
			 * If this is not an indirect operation don't try to
			 * free indirect segments
			 */
			goto free_shadow;

		for (j = 0; j < VSCSI_INDIRECT_PAGES(segs); j++) {
			persistent_gnt = info->shadow[i].indirect_grants[j];
			gnttab_end_foreign_access(persistent_gnt->gref, 0UL);
			__free_page(pfn_to_page(persistent_gnt->pfn));
			kfree(persistent_gnt);
		}

free_shadow:
		kfree(info->shadow[i].grants_used);
		info->shadow[i].grants_used = NULL;
		kfree(info->shadow[i].indirect_grants);
		info->shadow[i].indirect_grants = NULL;
	}

	scsifront_free_ring(info);
}

static void scsifront_free(struct vscsifrnt_info *info)
{
	struct Scsi_Host *host = info->host;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
	if (host->shost_state != SHOST_DEL) {
#else
	if (!test_bit(SHOST_DEL, &host->shost_state)) {
#endif
		scsi_remove_host(info->host);
	}

	scsifront_resume_free(info);

	scsi_host_put(info->host);
}

static void shadow_init(struct vscsifrnt_shadow * shadow, unsigned int ring_size)
{
	unsigned int i = 0;

	WARN_ON(!ring_size);

	for (i = 0; i < ring_size; i++) {
		shadow[i].next_free = i + 1;
		init_waitqueue_head(&(shadow[i].wq_reset));
		shadow[i].wait_reset = 0;
	}
	shadow[ring_size - 1].next_free = VSCSIIF_NONE;
}

static int scsifront_alloc_ring(struct vscsifrnt_info *info)
{
	struct xenbus_device *dev = info->dev;
	struct vscsiif_sring *sring;
	int err = -ENOMEM;
	int i;

	for (i = 0; i < info->ring_size; i++)
		info->ring_ref[i] = GRANT_INVALID_REF;

	/***** Frontend to Backend ring start *****/
	sring = (struct vscsiif_sring *) __get_free_pages(GFP_KERNEL, 
					get_order(VSCSIIF_MAX_RING_PAGE_SIZE));
	if (!sring) {
		xenbus_dev_fatal(dev, err, "fail to allocate shared ring (Front to Back)");
		return err;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&info->ring, sring, (unsigned long)info->ring_size << PAGE_SHIFT);

	for (i = 0; i < info->ring_size; i++) {
		err = xenbus_grant_ring(dev, virt_to_mfn((unsigned long)sring + i * PAGE_SIZE));
		if (err < 0) {
			info->ring.sring = NULL;
			xenbus_dev_fatal(dev, err, "fail to grant shared ring (Front to Back)");
			goto free_sring;
		}
		info->ring_ref[i] = err;
	}

	err = bind_listening_port_to_irqhandler(
			dev->otherend_id, scsifront_intr,
			0, "scsifront", info);

	if (err <= 0) {
		xenbus_dev_fatal(dev, err, "bind_listening_port_to_irqhandler");
		goto free_sring;
	}
	info->irq = err;

	return 0;

/* free resource */
free_sring:
	scsifront_free(info);

	return err;
}


static int scsifront_init_ring(struct vscsifrnt_info *info)
{
	struct xenbus_device *dev = info->dev;
	struct xenbus_transaction xbt;
	unsigned int ring_size, ring_order;
	const char *what = NULL;
	int err;
	int i;
	char buf[16];

	DPRINTK("%s\n",__FUNCTION__);

	err = xenbus_scanf(XBT_NIL, dev->otherend,
			   "max-ring-page-order", "%u", &ring_order);
	if (err != 1)
		ring_order = VSCSIIF_MAX_RING_ORDER;

	if (ring_order > VSCSIIF_MAX_RING_ORDER)
		ring_order = VSCSIIF_MAX_RING_ORDER;
	/*
	 * While for larger rings not all pages are actually used, be on the
	 * safe side and set up a full power of two to please as many backends
	 * as possible.
	 */
	printk("talk_to_scsiback ring_size %d, ring_order %d\n", 1U<<ring_order, ring_order);
	info->ring_size = ring_size = 1U << ring_order;

	/* Create shared ring, alloc event channel. */
	err = scsifront_alloc_ring(info);
	if (err)
		return err;

again:
	err = xenbus_transaction_start(&xbt);
	if (err) {
		xenbus_dev_fatal(dev, err, "starting transaction");
	}

	/*
	 * for compatibility with old backend
	 * ring order = 0 write the same xenstore
	 * key with ring order > 0
	 */
	what = "ring-page-order";
	err = xenbus_printf(xbt, dev->nodename, what, "%u",
			    ring_order);
	if (err)
		goto fail;
	what = "num-ring-pages";
	err = xenbus_printf(xbt, dev->nodename, what, "%u",
				ring_size);
	if (err)
		goto fail;
	what = buf;
	for (i = 0; i < ring_size; i++) {
		snprintf(buf, sizeof(buf), "ring-ref-%d", i + 1);
		err = xenbus_printf(xbt, dev->nodename, what, "%u",
		                        info->ring_ref[i]);
		if (err)
			goto fail;
	}

	what = "event-channel";
	err = xenbus_printf(xbt, dev->nodename, what, "%u",
				irq_to_evtchn_port(info->irq));
	if (err)
		goto fail;

	/* for persistent grant */
	what = "feature-persistent";
	err = xenbus_printf(xbt, dev->nodename,
			    what, "%u", 1);
	if (err)
		goto fail;

	err = xenbus_transaction_end(xbt, 0);
	if (err) {
		if (err == -EAGAIN)
			goto again;
		xenbus_dev_fatal(dev, err, "completing transaction");
		goto free_sring;
	}

	return 0;

fail:
	xenbus_transaction_end(xbt, 1);
	if (what)
		xenbus_dev_fatal(dev, err, "writing %s", what);
free_sring:
	/* free resource */
	scsifront_free(info);
	
	return err;
}


static int scsifront_probe(struct xenbus_device *dev,
				const struct xenbus_device_id *id)
{
	struct vscsifrnt_info *info;
	struct Scsi_Host *host;
	int err = -ENOMEM;
	char name[DEFAULT_TASK_COMM_LEN];

	host = scsi_host_alloc(&scsifront_sht, sizeof(*info));
			       //offsetof(struct vscsifrnt_info,
					//active.segs[max_nr_segs]));
	if (!host) {
		xenbus_dev_fatal(dev, err, "fail to allocate scsi host");
		return err;
	}
	info = (struct vscsifrnt_info *) host->hostdata;
	info->host = host;

	INIT_LIST_HEAD(&info->grants);
	INIT_LIST_HEAD(&info->indirect_pages);

	dev_set_drvdata(&dev->dev, info);
	info->dev  = dev;

	init_waitqueue_head(&info->wq);
	init_waitqueue_head(&info->wq_sync);
	spin_lock_init(&info->shadow_lock);

	info->persistent_gnts_c = 0;
	info->pause = 0;
	info->callers = 0;
	info->waiting_pause = 0;
	init_waitqueue_head(&info->wq_pause);

	snprintf(name, DEFAULT_TASK_COMM_LEN, "vscsiif.%d", info->host->host_no);

	info->kthread = kthread_run(scsifront_schedule, info, name);
	if (IS_ERR(info->kthread)) {
		err = PTR_ERR(info->kthread);
		info->kthread = NULL;
		dev_err(&dev->dev, "kthread start err %d\n", err);
		goto free_sring;
	}

	host->max_id      = VSCSIIF_MAX_TARGET;
	host->max_channel = 0;
	host->max_lun     = VSCSIIF_MAX_LUN;
	host->max_sectors = (host->sg_tablesize - 1) * PAGE_SIZE / 512;
	host->max_cmd_len = VSCSIIF_MAX_COMMAND_SIZE;

	err = scsi_add_host(host, &dev->dev);
	if (err) {
		dev_err(&dev->dev, "fail to add scsi host %d\n", err);
		goto free_sring;
	}

	return 0;

free_sring:
	/* free resource */
	scsifront_free(info);
	return err;
}

static int scsifront_resume(struct xenbus_device *dev)
{
	struct vscsifrnt_info *info = dev_get_drvdata(&dev->dev);
	struct Scsi_Host *host = info->host;
	int err;

	spin_lock_irq(host->host_lock);

	/* finish all still pending commands */
	scsifront_finish_all(info);

	spin_unlock_irq(host->host_lock);

	/* reconnect to dom0 */
	scsifront_resume_free(info);
	err = scsifront_init_ring(info);
	if (err) {
		dev_err(&dev->dev, "fail to resume %d\n", err);
		scsifront_free(info);
		return err;
	}

	shadow_init(info->shadow, RING_SIZE(&info->ring));
	info->shadow_free = info->ring.req_prod_pvt;
	xenbus_switch_state(dev, XenbusStateInitialised);

	return 0;
}

static int scsifront_suspend(struct xenbus_device *dev)
{
	struct vscsifrnt_info *info = dev_get_drvdata(&dev->dev);
	struct Scsi_Host *host = info->host;
	int err = 0;

	/* no new commands for the backend */
	spin_lock_irq(host->host_lock);
	info->pause = 1;
	while (info->callers && !err) {
		info->waiting_pause = 1;
		info->waiting_sync = 0;
		spin_unlock_irq(host->host_lock);
		wake_up(&info->wq_sync);
		err = wait_event_interruptible(info->wq_pause,
					       !info->waiting_pause);
		spin_lock_irq(host->host_lock);
	}
	spin_unlock_irq(host->host_lock);
	return err;
}

static int scsifront_suspend_cancel(struct xenbus_device *dev)
{
	struct vscsifrnt_info *info = dev_get_drvdata(&dev->dev);
	struct Scsi_Host *host = info->host;

	spin_lock_irq(host->host_lock);
	info->pause = 0;
	spin_unlock_irq(host->host_lock);
	return 0;
}

static int scsifront_remove(struct xenbus_device *dev)
{
	struct vscsifrnt_info *info = dev_get_drvdata(&dev->dev);

	DPRINTK("%s: %s removed\n",__FUNCTION__ ,dev->nodename);

	if (info->kthread) {
		kthread_stop(info->kthread);
		info->kthread = NULL;
	}

	scsifront_free(info);
	
	return 0;
}


static int scsifront_disconnect(struct vscsifrnt_info *info)
{
	struct xenbus_device *dev = info->dev;
	struct Scsi_Host *host = info->host;

	DPRINTK("%s: %s disconnect\n",__FUNCTION__ ,dev->nodename);

	/* 
	  When this function is executed,  all devices of 
	  Frontend have been deleted. 
	  Therefore, it need not block I/O before remove_host.
	*/

	scsi_remove_host(host);
	xenbus_frontend_closed(dev);

	return 0;
}

#define VSCSIFRONT_OP_ADD_LUN	1
#define VSCSIFRONT_OP_DEL_LUN	2
#define VSCSIFRONT_OP_READD_LUN	3

static void scsifront_do_lun_hotplug(struct vscsifrnt_info *info, int op)
{
	struct xenbus_device *dev = info->dev;
	int i, err = 0;
	char str[64], state_str[64];
	char **dir;
	unsigned int dir_n = 0;
	unsigned int device_state;
	unsigned int hst, chn, tgt, lun;
	struct scsi_device *sdev;

	dir = xenbus_directory(XBT_NIL, dev->otherend, "vscsi-devs", &dir_n);
	if (IS_ERR(dir))
		return;

	for (i = 0; i < dir_n; i++) {
		/* read status */
		snprintf(str, sizeof(str), "vscsi-devs/%s/state", dir[i]);
		err = xenbus_scanf(XBT_NIL, dev->otherend, str, "%u",
			&device_state);
		if (XENBUS_EXIST_ERR(err))
			continue;
		
		/* virtual SCSI device */
		snprintf(str, sizeof(str), "vscsi-devs/%s/v-dev", dir[i]);
		err = xenbus_scanf(XBT_NIL, dev->otherend, str,
			"%u:%u:%u:%u", &hst, &chn, &tgt, &lun);
		if (XENBUS_EXIST_ERR(err))
			continue;

		/* front device state path */
		snprintf(state_str, sizeof(state_str), "vscsi-devs/%s/state", dir[i]);

		switch (op) {
		case VSCSIFRONT_OP_ADD_LUN:
			if (device_state == XenbusStateInitialised) {
				sdev = scsi_device_lookup(info->host, chn, tgt, lun);
				if (sdev) {
					dev_err(&dev->dev, "Device already in use.\n");
					scsi_device_put(sdev);
					xenbus_printf(XBT_NIL, dev->nodename,
						state_str, "%d", XenbusStateClosed);
				} else {
					scsi_add_device(info->host, chn, tgt, lun);
					xenbus_printf(XBT_NIL, dev->nodename,
						state_str, "%d", XenbusStateConnected);
				}
			}
			break;
		case VSCSIFRONT_OP_DEL_LUN:
			if (device_state == XenbusStateClosing) {
				sdev = scsi_device_lookup(info->host, chn, tgt, lun);
				if (sdev) {
					scsi_remove_device(sdev);
					scsi_device_put(sdev);
					xenbus_printf(XBT_NIL, dev->nodename,
						state_str, "%d", XenbusStateClosed);
				}
			}
			break;
		case VSCSIFRONT_OP_READD_LUN:
			if (device_state == XenbusStateConnected)
				xenbus_printf(XBT_NIL, dev->nodename, state_str,
					      "%d", XenbusStateConnected);
			break;
		default:
			break;
		}
	}
	
	kfree(dir);
	return;
}

static void scsifront_setup_persistent(struct vscsifrnt_info *info)
{
	int err;
	int persistent;

	err = xenbus_gather(XBT_NIL, info->dev->otherend,
			    "feature-persistent", "%u", &persistent,
			    NULL);
	if (err)
		info->feature_persistent = 0;
	else
		info->feature_persistent = persistent;
}

static void scsifront_setup_host(struct vscsifrnt_info *info)
{
	int nr_segs;
	struct Scsi_Host *host = info->host;

	if (info->max_indirect_segments)
		nr_segs = info->max_indirect_segments;
	else
		nr_segs = VSCSIIF_SG_TABLESIZE;

	if (!info->pause && nr_segs > host->sg_tablesize) {
		host->sg_tablesize = nr_segs;
		dev_info(&info->dev->dev, "using up to %d SG entries\n",
			 host->sg_tablesize);
		host->max_sectors = (host->sg_tablesize - 1) * PAGE_SIZE / 512;
	} else if (info->pause && nr_segs < host->sg_tablesize)
		dev_warn(&info->dev->dev,
			 "SG entries decreased from %d to %u - device may not work properly anymore\n",
			 host->sg_tablesize, nr_segs);

}

static int scsifront_setup_indirect(struct vscsifrnt_info *info)
{
	unsigned int indirect_segments, segs;
	int err, i;

	info->max_indirect_segments = 0;
	segs = VSCSIIF_SG_TABLESIZE;

	err = xenbus_gather(XBT_NIL, info->dev->otherend,
		"feature-max-indirect-segments", "%u", &indirect_segments,
		NULL);

	if (err != 1)
		segs = VSCSIIF_SG_TABLESIZE;

	if (!err && indirect_segments > VSCSIIF_SG_TABLESIZE) {
		info->max_indirect_segments = min(indirect_segments, MAX_VSCSI_INDIRECT_SEGMENTS);
		segs = info->max_indirect_segments;
	}

	scsifront_setup_host(info);

	printk("[%s:%d], segs %d\n", __func__, __LINE__, segs);

	err = fill_grant_buffer(info, (segs + VSCSI_INDIRECT_PAGES(segs)) * RING_SIZE(&info->ring));
	if (err)
		goto out_of_memory;

	if (!info->feature_persistent && info->max_indirect_segments) {
		/*
		 * We are using indirect descriptors but not persistent
		 * grants, we need to allocate a set of pages that can be
		 * used for mapping indirect grefs
		 */
		int num = VSCSI_INDIRECT_PAGES(segs) * RING_SIZE(&info->ring);

		BUG_ON(!list_empty(&info->indirect_pages));
		for (i = 0; i < num; i++) {
			struct page *indirect_page = alloc_page(GFP_NOIO);
			if (!indirect_page)
				goto out_of_memory;
			list_add(&indirect_page->lru, &info->indirect_pages);
		}
	}

	for (i = 0; i < RING_SIZE(&info->ring); i++) {
		info->shadow[i].grants_used = kzalloc(
			sizeof(info->shadow[i].grants_used[0]) * segs,
			GFP_NOIO);
		if (info->max_indirect_segments)
			info->shadow[i].indirect_grants = kzalloc(
				sizeof(info->shadow[i].indirect_grants[0]) *
				VSCSI_INDIRECT_PAGES(segs),
				GFP_NOIO);
		if ((info->shadow[i].grants_used == NULL) ||
			(info->max_indirect_segments &&
			(info->shadow[i].indirect_grants == NULL)))
			goto out_of_memory;
	}

	return 0;

out_of_memory:
	for (i = 0; i < RING_SIZE(&info->ring); i++) {
		kfree(info->shadow[i].grants_used);
		info->shadow[i].grants_used = NULL;
		kfree(info->shadow[i].indirect_grants);
		info->shadow[i].indirect_grants = NULL;
	}
	if (!list_empty(&info->indirect_pages)) {
		struct page *indirect_page, *n;
		list_for_each_entry_safe(indirect_page, n, &info->indirect_pages, lru) {
			list_del(&indirect_page->lru);
			__free_page(indirect_page);
		}
	}
	return -ENOMEM;
}

static void scsifront_backend_changed(struct xenbus_device *dev,
				enum xenbus_state backend_state)
{
	struct vscsifrnt_info *info = dev_get_drvdata(&dev->dev);
	int err;

	DPRINTK("%p %u %u\n", dev, dev->state, backend_state);

	switch (backend_state) {
	case XenbusStateUnknown:
	case XenbusStateInitialising:
		break;
	case XenbusStateInitWait:
		if (dev->state == XenbusStateInitialised)
			break;
		err = scsifront_init_ring(info);
		if (err) {
			scsi_host_put(info->host);
			xenbus_dev_fatal(info->dev, err, "scsi init ring at %s",
				info->dev->otherend);
			return;
		}
		shadow_init(info->shadow, RING_SIZE(&info->ring));
		xenbus_switch_state(dev, XenbusStateInitialised);
		break;
	case XenbusStateInitialised:
		break;

	case XenbusStateConnected:
		if (xenbus_read_driver_state(dev->nodename) ==
			XenbusStateInitialised) {
			scsifront_setup_persistent(info);
			err = scsifront_setup_indirect(info);
			if (err) {
				xenbus_dev_fatal(info->dev, err, "setup_indirect at %s",
					info->dev->otherend);
				return;
			}
		}

		if (info->pause) {
			scsifront_do_lun_hotplug(info, VSCSIFRONT_OP_READD_LUN);
			xenbus_switch_state(dev, XenbusStateConnected);
			info->pause = 0;
			return;
		}

		if (xenbus_read_driver_state(dev->nodename) ==
			XenbusStateInitialised) {
			scsifront_do_lun_hotplug(info, VSCSIFRONT_OP_ADD_LUN);
		}
		
		if (dev->state == XenbusStateConnected)
			break;
			
		xenbus_switch_state(dev, XenbusStateConnected);
		break;

	case XenbusStateClosed:
		if (dev->state == XenbusStateClosed)
			break;
		/* Missed the backend's Closing state -- fallthrough */
	case XenbusStateClosing:
		scsifront_disconnect(info);
		break;

	case XenbusStateReconfiguring:
		scsifront_do_lun_hotplug(info, VSCSIFRONT_OP_DEL_LUN);
		xenbus_switch_state(dev, XenbusStateReconfiguring);
		break;

	case XenbusStateReconfigured:
		scsifront_do_lun_hotplug(info, VSCSIFRONT_OP_ADD_LUN);
		xenbus_switch_state(dev, XenbusStateConnected);
		break;
	}
}


static const struct xenbus_device_id scsifront_ids[] = {
	{ "vscsi" },
	{ "" }
};
MODULE_ALIAS("xen:vscsi");

static DEFINE_XENBUS_DRIVER(scsifront, ,
	.probe			= scsifront_probe,
	.remove			= scsifront_remove,
	.resume			= scsifront_resume,
	.suspend		= scsifront_suspend,
	.suspend_cancel		= scsifront_suspend_cancel,
	.otherend_changed	= scsifront_backend_changed,
);

int __init scsifront_xenbus_init(void)
{
	if (max_nr_segs > SG_ALL)
		max_nr_segs = SG_ALL;
	if (max_nr_segs < VSCSIIF_SG_TABLESIZE)
		max_nr_segs = VSCSIIF_SG_TABLESIZE;

	return xenbus_register_frontend(&scsifront_driver);
}

void __exit scsifront_xenbus_unregister(void)
{
	xenbus_unregister_driver(&scsifront_driver);
}

