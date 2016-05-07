/*
 * blkfront.c
 *
 * XenLinux virtual block device driver.
 *
 * Copyright (c) 2003-2004, Keir Fraser & Steve Hand
 * Modifications by Mark A. Williamson are (c) Intel Research Cambridge
 * Copyright (c) 2004, Christian Limpach
 * Copyright (c) 2004, Andrew Warfield
 * Copyright (c) 2005, Christopher Clark
 * Copyright (c) 2005, XenSource Ltd
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

#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/kthread.h>

#include <xen/xenbus.h>
#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/page.h>
#include <xen/platform_pci.h>

#include <xen/interface/grant_table.h>
#ifdef ORACLE
#include <xen/blkif.h>
#else
#include <xen/interface/io/blkif.h>
#endif
#include <xen/interface/io/protocols.h>

#include <asm/xen/hypervisor.h>
#include <linux/pm.h>

#define UNPLUG_DISK_TMOUT   60

static int sda_is_xvda;
module_param(sda_is_xvda, bool, 0);
MODULE_PARM_DESC(sda_is_xvda,
		 "sdX in guest config translates to xvdX, not xvd(X+4)");

enum blkif_state {
	BLKIF_STATE_DISCONNECTED,
	BLKIF_STATE_CONNECTED,
	BLKIF_STATE_SUSPENDED,
};

struct grant {
	grant_ref_t gref;
	unsigned long pfn;
	struct list_head node;
};

struct blk_shadow {
	struct blkif_request req;
	struct request *request;
	struct grant **grants_used;
	struct grant **indirect_grants;
	/* If use persistent grant, it will copy response
	 * from grant page to sg when read io completion.
	 */
	struct scatterlist *sg;
};

struct split_bio {
	struct bio *bio;
	atomic_t pending;
	int err;
};

static const struct block_device_operations xlvbd_block_fops;

/*
 * Maximum number of segments in indirect requests, the actual value used by
 * the frontend driver is the minimum of this value and the value provided
 * by the backend driver.
 */

static unsigned int xen_blkif_max_segments = 256;
module_param_named(max, xen_blkif_max_segments, int, S_IRUGO);
MODULE_PARM_DESC(max, "Maximum amount of segments in indirect requests (default is 256 (1M))");

//#define BLK_RING_SIZE __RING_SIZE((struct blkif_sring *)0, PAGE_SIZE)
#define BLK_MAX_RING_PAGE_ORDER 4U
#define BLK_MAX_RING_PAGES (1U << BLK_MAX_RING_PAGE_ORDER)
#define BLK_MAX_RING_SIZE __CONST_RING_SIZE(blkif, \
						BLK_MAX_RING_PAGES * PAGE_SIZE)
//#define BLK_RING_SIZE __RING_SIZE((struct blkif_sring *)0, PAGE_SIZE)

/*
 * We have one of these per vbd, whether ide, scsi or 'other'.  They
 * hang in private_data off the gendisk structure. We may end up
 * putting all kinds of interesting stuff here :-)
 */
struct blkfront_info
{
	struct mutex mutex;
	struct xenbus_device *xbdev;
	struct gendisk *gd;
	int vdevice;
	blkif_vdev_t handle;
	enum blkif_state connected;
	unsigned int ring_size;
	struct blkif_front_ring ring;
	/* move sg to blk_shadow struct */
	//struct scatterlist *sg;
	unsigned int evtchn, irq;
#if defined(ORACLE)
	struct tasklet_struct tasklet;
#endif
	struct request_queue *rq;
	struct work_struct work;
	struct gnttab_free_callback callback;
	struct blk_shadow shadow[BLK_MAX_RING_SIZE];
	//for hot-migrate
	struct blk_shadow shadow_copy[BLK_MAX_RING_SIZE];
	grant_ref_t ring_refs[BLK_MAX_RING_PAGES];
	struct page *ring_pages[BLK_MAX_RING_PAGES];
	//for hot-migrate
	//struct page *ring_pages_copy[BLK_MAX_RING_PAGES];
	struct list_head grants;
	struct list_head indirect_pages;
	unsigned int persistent_gnts_c;
	unsigned int feature_persistent:1;
	unsigned long shadow_free;
#if defined(ORACLE)
	int feature_barrier;
#endif
	unsigned int feature_flush;
	unsigned int max_indirect_segments;
	int is_ready;
#if defined(ORACLE)
	spinlock_t io_lock;
#endif
	int users;
	int blkif_detach;
};

static DEFINE_SPINLOCK(blkif_io_lock);

static unsigned int nr_minors;
static unsigned long *minors;
static DEFINE_SPINLOCK(minor_lock);

#define MAXIMUM_OUTSTANDING_BLOCK_REQS \
	(BLKIF_MAX_SEGMENTS_PER_REQUEST * BLK_RING_SIZE)
#define GRANT_INVALID_REF	0

#define PARTS_PER_DISK		16
#define PARTS_PER_EXT_DISK      256

#define BLKIF_MAJOR(dev) ((dev)>>8)
#define BLKIF_MINOR(dev) ((dev) & 0xff)

#define EXT_SHIFT 28
#define EXTENDED (1<<EXT_SHIFT)
#define VDEV_IS_EXTENDED(dev) ((dev)&(EXTENDED))
#define BLKIF_MINOR_EXT(dev) ((dev)&(~EXTENDED))
#define EMULATED_HD_DISK_MINOR_OFFSET (0)
#define EMULATED_HD_DISK_NAME_OFFSET (EMULATED_HD_DISK_MINOR_OFFSET / 256)

#define DEV_NAME	"xvd"	/* name in /dev */

#define SEGS_PER_INDIRECT_FRAME \
	(PAGE_SIZE/sizeof(struct blkif_request_segment_aligned))
#define INDIRECT_GREFS(_segs) \
	((_segs + SEGS_PER_INDIRECT_FRAME - 1)/SEGS_PER_INDIRECT_FRAME)

static int blkfront_setup_indirect(struct blkfront_info *info);
static int blkif_recover(struct blkfront_info *info);

/* module settings dependent on the "sda_is_xvda" module parameter */
static int emulated_sd_disk_minor_offset = EMULATED_HD_DISK_MINOR_OFFSET + (4 * 16);
static int emulated_sd_disk_name_offset = EMULATED_HD_DISK_NAME_OFFSET + 4;

static int get_id_from_freelist(struct blkfront_info *info)
{
	unsigned long free = info->shadow_free;
	BUG_ON(free >= RING_SIZE(&info->ring));
	info->shadow_free = info->shadow[free].req.u.rw.id;
	info->shadow[free].req.u.rw.id = 0x0fffffee; /* debug */
	return free;
}

static void add_id_to_freelist(struct blkfront_info *info,
				   unsigned long id)
{
	info->shadow[id].req.u.rw.id  = info->shadow_free;
	info->shadow[id].request = NULL;
	info->shadow_free = id;
}


static int fill_grant_buffer(struct blkfront_info *info, int num)
{
	struct page *granted_page;
	struct grant *gnt_list_entry, *n;
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

static struct grant *get_grant(grant_ref_t *gref_head,
							   unsigned long pfn,
							   struct blkfront_info *info)
{
	struct grant *gnt_list_entry;
	unsigned long buffer_mfn;

	BUG_ON(list_empty(&info->grants));
	gnt_list_entry = list_first_entry(&info->grants, struct grant,
									  node);
	list_del(&gnt_list_entry->node);

	/* for persistent grant */
	if (gnt_list_entry->gref != GRANT_INVALID_REF) {
		info->persistent_gnts_c--;
		return gnt_list_entry;
	}

	/* Assign a gref to this page */
	gnt_list_entry->gref = gnttab_claim_grant_reference(gref_head);
	BUG_ON(gnt_list_entry->gref == -ENOSPC);
	if (!info->feature_persistent) {
		BUG_ON(!pfn);
		gnt_list_entry->pfn = pfn;
	}
	buffer_mfn = pfn_to_mfn(gnt_list_entry->pfn);
	gnttab_grant_foreign_access_ref(gnt_list_entry->gref,
									info->xbdev->otherend_id,
									buffer_mfn, 0);
	return gnt_list_entry;
}

static int xlbd_reserve_minors(unsigned int minor, unsigned int nr)
{
	unsigned int end = minor + nr;
	int rc;

	if (end > nr_minors) {
		unsigned long *bitmap, *old;

		bitmap = kzalloc(BITS_TO_LONGS(end) * sizeof(*bitmap),
				 GFP_KERNEL);
		if (bitmap == NULL)
			return -ENOMEM;

		spin_lock(&minor_lock);
		if (end > nr_minors) {
			old = minors;
			memcpy(bitmap, minors,
				   BITS_TO_LONGS(nr_minors) * sizeof(*bitmap));
			minors = bitmap;
			nr_minors = BITS_TO_LONGS(end) * BITS_PER_LONG;
		} else
			old = bitmap;
		spin_unlock(&minor_lock);
		kfree(old);
	}

	spin_lock(&minor_lock);
	if (find_next_bit(minors, end, minor) >= end) {
		for (; minor < end; ++minor)
			__set_bit(minor, minors);
		rc = 0;
	} else
		rc = -EBUSY;
	spin_unlock(&minor_lock);

	return rc;
}

static void xlbd_release_minors(unsigned int minor, unsigned int nr)
{
	unsigned int end = minor + nr;

	BUG_ON(end > nr_minors);
	spin_lock(&minor_lock);
	for (; minor < end; ++minor)
		__clear_bit(minor, minors);
	spin_unlock(&minor_lock);
}

static void blkif_restart_queue_callback(void *arg)
{
	struct blkfront_info *info = (struct blkfront_info *)arg;
	schedule_work(&info->work);
}

static int blkif_getgeo(struct block_device *bd, struct hd_geometry *hg)
{
	/* We don't have real geometry info, but let's at least return
	   values consistent with the size of the device */
	sector_t nsect = get_capacity(bd->bd_disk);
	sector_t cylinders = nsect;

	hg->heads = 0xff;
	hg->sectors = 0x3f;
	sector_div(cylinders, hg->heads * hg->sectors);
	hg->cylinders = cylinders;
	if ((sector_t)(hg->cylinders + 1) * hg->heads * hg->sectors < nsect)
		hg->cylinders = 0xffff;
	return 0;
}

static int blkif_ioctl(struct block_device *bdev, fmode_t mode,
			   unsigned command, unsigned long argument)
{
	struct blkfront_info *info = bdev->bd_disk->private_data;
	int i;

	dev_dbg(&info->xbdev->dev, "command: 0x%x, argument: 0x%lx\n",
		command, (long)argument);

	switch (command) {
	case CDROMMULTISESSION:
		dev_dbg(&info->xbdev->dev, "FIXME: support multisession CDs later\n");
		for (i = 0; i < sizeof(struct cdrom_multisession); i++)
			if (put_user(0, (char __user *)(argument + i)))
				return -EFAULT;
		return 0;

	case CDROM_GET_CAPABILITY: {
		struct gendisk *gd = info->gd;
		if (gd->flags & GENHD_FL_CD)
			return 0;
		return -EINVAL;
	}

	default:
		/*printk(KERN_ALERT "ioctl %08x not supported by Xen blkdev\n",
		  command);*/
		return -EINVAL; /* same return as native Linux */
	}

	return 0;
}

/*
 * blkif_queue_request
 *
 * request block io
 *
 * id: for guest use only.
 * operation: BLKIF_OP_{READ,WRITE,PROBE}
 * buffer: buffer to read/write into. this should be a
 *   virtual address in the guest os.
 */
static int blkif_queue_request(struct request *req)
{
	struct blkfront_info *info = req->rq_disk->private_data;
	struct blkif_request *ring_req;
	unsigned long id;
	unsigned int fsect, lsect;
	int i, ref, n;
	struct blkif_request_segment_aligned *segments = NULL;

	/*
	 * Used to store if we are able to queue the request by just using
	 * existing persistent grants, or if we have to get new grants,
	 * as there are not sufficiently many free.
	 */
	bool new_persistent_gnts;
	grant_ref_t gref_head;
	struct grant *gnt_list_entry = NULL;
	struct scatterlist *sg;
	int nseg, max_grefs;

	if (unlikely(info->connected != BLKIF_STATE_CONNECTED))
		return 1;

	max_grefs = info->max_indirect_segments ?
		info->max_indirect_segments +
		INDIRECT_GREFS(info->max_indirect_segments) :
		BLKIF_MAX_SEGMENTS_PER_REQUEST;

	/* Check if we have enought grants to allocate a requests */
	if (info->persistent_gnts_c < max_grefs) {
		new_persistent_gnts = 1;
		if (gnttab_alloc_grant_references(
			max_grefs - info->persistent_gnts_c,
			&gref_head) < 0) {
			gnttab_request_free_callback(
				&info->callback,
				blkif_restart_queue_callback,
				info,
				max_grefs);
			return 1;
		}
	} else
		new_persistent_gnts = 0;

	/* Fill out a communications ring structure. */
	ring_req = RING_GET_REQUEST(&info->ring, info->ring.req_prod_pvt);
	id = get_id_from_freelist(info);
	info->shadow[id].request = req;

	BUG_ON(info->max_indirect_segments == 0 &&
				req->nr_phys_segments > BLKIF_MAX_SEGMENTS_PER_REQUEST);
	BUG_ON(info->max_indirect_segments &&
				req->nr_phys_segments > info->max_indirect_segments);

	nseg = blk_rq_map_sg(req->q, req, info->shadow[id].sg);
	ring_req->u.rw.id = id;

	if (nseg > BLKIF_MAX_SEGMENTS_PER_REQUEST) {
	/*
	* The indirect operation can only be a BLKIF_OP_READ or
	* BLKIF_OP_WRITE
	*/
#ifndef ORACLE
		BUG_ON(req->cmd_flags & (REQ_FLUSH | REQ_FUA));
#endif
		ring_req->operation = BLKIF_OP_INDIRECT;
		ring_req->u.indirect.indirect_op = rq_data_dir(req) ?
			BLKIF_OP_WRITE : BLKIF_OP_READ;
		ring_req->u.indirect.sector_number = (blkif_sector_t)blk_rq_pos(req);
		ring_req->u.indirect.handle = info->handle;
		ring_req->u.indirect.nr_segments = nseg;
	} else {
		ring_req->u.rw.sector_number = (blkif_sector_t)blk_rq_pos(req);
		ring_req->u.rw.handle = info->handle;
		ring_req->operation = rq_data_dir(req) ?
			BLKIF_OP_WRITE : BLKIF_OP_READ;
		if (req->cmd_flags & REQ_HARDBARRIER)
			ring_req->operation = BLKIF_OP_WRITE_BARRIER;
		ring_req->u.rw.nr_segments = nseg;
	}
	for_each_sg(info->shadow[id].sg, sg, nseg, i) {
		fsect = sg->offset >> 9;
		lsect = fsect + (sg->length >> 9) - 1;

		if ((ring_req->operation == BLKIF_OP_INDIRECT) &&
			(i % SEGS_PER_INDIRECT_FRAME == 0)) {
			unsigned long uninitialized_var(pfn);

			if (segments)
				kunmap_atomic(segments, KM_USER0);

			n = i / SEGS_PER_INDIRECT_FRAME;
			if (!info->feature_persistent) {
				struct page *indirect_page;
				/* Fetch a pre-allocated page to use for indirect grefs */
				BUG_ON(list_empty(&info->indirect_pages));
				indirect_page = list_first_entry(&info->indirect_pages,
												 struct page, lru);
				list_del(&indirect_page->lru);
				pfn = page_to_pfn(indirect_page);
			}
			gnt_list_entry = get_grant(&gref_head, pfn, info);
			info->shadow[id].indirect_grants[n] = gnt_list_entry;
			segments = kmap_atomic(pfn_to_page(gnt_list_entry->pfn), KM_USER0);
			ring_req->u.indirect.indirect_grefs[n] = gnt_list_entry->gref;
		}

		gnt_list_entry = get_grant(&gref_head, page_to_pfn(sg_page(sg)), info);
		ref = gnt_list_entry->gref;

		info->shadow[id].grants_used[i] = gnt_list_entry;

		/* If use persistent grant, it will have a memcpy,
		 * just copy the data from sg page to grant page. 
		 */
		if (rq_data_dir(req) && info->feature_persistent) {
			char *bvec_data;
			void *shared_data;

			BUG_ON(sg->offset + sg->length > PAGE_SIZE);

			shared_data = kmap_atomic(pfn_to_page(gnt_list_entry->pfn), KM_USER0);

			if (in_interrupt()) {
				if (in_irq())
					bvec_data = kmap_atomic(sg_page(sg), KM_IRQ0);
				else if (in_softirq())
					bvec_data = kmap_atomic(sg_page(sg), KM_SOFTIRQ0);
				else
					printk(KERN_WARNING "[kmap] in interrupt, but not irq!\n");
			}
			else
				bvec_data = kmap_atomic(sg_page(sg), KM_USER0);

			/*
			 * this does not wipe data stored outside the
			 * range sg->offset..sg->offset+sg->length.
			 * Therefore, blkback *could* see data from
			 * previous requests. This is OK as long as
			 * persistent grants are shared with just one
			 * domain. It may need refactoring if this
			 * changes
			 */
			memcpy(shared_data + sg->offset,
				   bvec_data   + sg->offset,
				   sg->length);

			if (in_interrupt()) {
				if (in_irq())
					kunmap_atomic(bvec_data, KM_IRQ0);
				else if (in_softirq())
					kunmap_atomic(bvec_data, KM_SOFTIRQ0);
				else
					printk(KERN_WARNING "[kunmap] in interrupt, but not irq!\n");
			}
			else
				kunmap_atomic(bvec_data, KM_USER0);
			kunmap_atomic(shared_data, KM_USER0);
		}
		if (ring_req->operation != BLKIF_OP_INDIRECT) {
			ring_req->u.rw.seg[i] =
				(struct blkif_request_segment) {
					.gref       = ref,
					.first_sect = fsect,
					.last_sect  = lsect };
		} else {
			n = i % SEGS_PER_INDIRECT_FRAME;
			segments[n] =
				(struct blkif_request_segment_aligned) {
					.gref       = ref,
					.first_sect = fsect,
					.last_sect  = lsect };
		}
	}		

	if (segments)
		kunmap_atomic(segments, KM_USER0);

	info->ring.req_prod_pvt++;

	/* Keep a private copy so we can reissue requests when recovering. */
	info->shadow[id].req = *ring_req;

	/* for persistent grant */
	if (new_persistent_gnts)
		gnttab_free_grant_references(gref_head);

	return 0;
}


static inline void flush_requests(struct blkfront_info *info)
{
	int notify;

	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(&info->ring, notify);

	if (notify)
		notify_remote_via_irq(info->irq);
}

/*
 * do_blkif_request
 *  read a block; request is in a request queue
 */
static void do_blkif_request(struct request_queue *rq)
{
	struct blkfront_info *info = NULL;
	struct request *req;
	int queued;

	pr_debug("Entered do_blkif_request\n");

	queued = 0;

	while ((req = blk_peek_request(rq)) != NULL) {
		info = req->rq_disk->private_data;

		if (RING_FULL(&info->ring))
			goto wait;

		blk_start_request(req);

		if (req->cmd_type != REQ_TYPE_FS) {
			__blk_end_request_all(req, -EIO);
			continue;
		}

		if ( info->blkif_detach != 0)
		{
			if( rq_data_dir(req) ){
				__blk_end_request_all(req, -ENOSPC);
			}
			else{
				__blk_end_request_all(req, -EIO);
			}
			continue;
		}

		pr_debug("do_blk_req %p: cmd %p, sec %lx, "
			 "(%u/%u) buffer:%p [%s]\n",
			 req, req->cmd, (unsigned long)blk_rq_pos(req),
			 blk_rq_cur_sectors(req), blk_rq_sectors(req),
			 req->buffer, rq_data_dir(req) ? "write" : "read");

		if (blkif_queue_request(req)) {
			blk_requeue_request(rq, req);
wait:
			/* Avoid pointless unplugs. */
			blk_stop_queue(rq);
			break;
		}

		queued++;
	}

	if (queued != 0)
		flush_requests(info);
}

static int xlvbd_init_blk_queue(struct gendisk *gd, u16 sector_size,
					unsigned int segments)
{
	struct request_queue *rq;

	rq = blk_init_queue(do_blkif_request, &blkif_io_lock);
	if (rq == NULL)
		return -1;

	queue_flag_set_unlocked(QUEUE_FLAG_VIRT, rq);

	/* Hard sector size and max sectors impersonate the equiv. hardware. */
	blk_queue_logical_block_size(rq, sector_size);
	blk_queue_max_hw_sectors(rq, segments*8);

	/* Each segment in a request is up to an aligned page in size. */
	blk_queue_segment_boundary(rq, PAGE_SIZE - 1);
	blk_queue_max_segment_size(rq, PAGE_SIZE);

	/* Ensure a merged request will fit in a single I/O ring slot. */
#if defined(ORACLE)
	blk_queue_max_phys_segments(rq, segments);
	blk_queue_max_hw_segments(rq, segments);
#else
	blk_queue_max_segments(rq, segments);
#endif
	printk("init queue set max_hw_sectors %d max_segments %d\n", segments*8, segments);

	/* Make sure buffer addresses are sector-aligned. */
	blk_queue_dma_alignment(rq, 511);

	/* Make sure we don't use bounce buffers. */
	blk_queue_bounce_limit(rq, BLK_BOUNCE_ANY);

	gd->queue = rq;

	return 0;
}

#if defined(ORACLE)
static int xlvbd_barrier(struct blkfront_info *info)
{
	int err;
	const char *barrier;

	switch (info->feature_barrier) {
	case QUEUE_ORDERED_DRAIN:	barrier = "enabled (drain)"; break;
	case QUEUE_ORDERED_TAG:		barrier = "enabled (tag)"; break;
	case QUEUE_ORDERED_NONE:	barrier = "disabled"; break;
	default:			return -EINVAL;
	}

	err = blk_queue_ordered(info->rq, info->feature_barrier, NULL);

	if (err)
		return err;

	printk(KERN_INFO "blkfront: %s: barriers %s\n",
		   info->gd->disk_name, barrier);
	return 0;
}
#else
static void xlvbd_flush(struct blkfront_info *info)
{
	blk_queue_flush(info->rq, info->feature_flush);
	printk(KERN_INFO "blkfront: %s: %s %s %s %s %s %s\n",
		   info->gd->disk_name,
		   "barriers",
		   info->feature_flush ? "enabled;" : "disabled;",
		   "persistent grants:",
		   info->feature_persistent ? "enabled;" : "disabled;",
		   "indirect descriptors:",
		   info->max_indirect_segments ? "enabled;" : "disabled;");
}
#endif

static int xen_translate_vdev(int vdevice, int *minor, unsigned int *offset)
{
	int major;
	major = BLKIF_MAJOR(vdevice);
	*minor = BLKIF_MINOR(vdevice);
	switch (major) {
		case XEN_IDE0_MAJOR:
			*offset = (*minor / 64) + EMULATED_HD_DISK_NAME_OFFSET;
			*minor = ((*minor / 64) * PARTS_PER_DISK) +
				EMULATED_HD_DISK_MINOR_OFFSET;
			break;
		case XEN_IDE1_MAJOR:
			*offset = (*minor / 64) + 2 + EMULATED_HD_DISK_NAME_OFFSET;
			*minor = (((*minor / 64) + 2) * PARTS_PER_DISK) +
				EMULATED_HD_DISK_MINOR_OFFSET;
			break;
		case XEN_SCSI_DISK0_MAJOR:
			*offset = (*minor / PARTS_PER_DISK) + emulated_sd_disk_name_offset;
			*minor = *minor + emulated_sd_disk_minor_offset;
			break;
		case XEN_SCSI_DISK1_MAJOR:
		case XEN_SCSI_DISK2_MAJOR:
		case XEN_SCSI_DISK3_MAJOR:
		case XEN_SCSI_DISK4_MAJOR:
		case XEN_SCSI_DISK5_MAJOR:
		case XEN_SCSI_DISK6_MAJOR:
		case XEN_SCSI_DISK7_MAJOR:
			*offset = (*minor / PARTS_PER_DISK) + 
				((major - XEN_SCSI_DISK1_MAJOR + 1) * 16) +
				emulated_sd_disk_name_offset;
			*minor = *minor +
				((major - XEN_SCSI_DISK1_MAJOR + 1) * 16 * PARTS_PER_DISK) +
				emulated_sd_disk_minor_offset;
			break;
		case XEN_SCSI_DISK8_MAJOR:
		case XEN_SCSI_DISK9_MAJOR:
		case XEN_SCSI_DISK10_MAJOR:
		case XEN_SCSI_DISK11_MAJOR:
		case XEN_SCSI_DISK12_MAJOR:
		case XEN_SCSI_DISK13_MAJOR:
		case XEN_SCSI_DISK14_MAJOR:
		case XEN_SCSI_DISK15_MAJOR:
			*offset = (*minor / PARTS_PER_DISK) + 
				((major - XEN_SCSI_DISK8_MAJOR + 8) * 16) +
				emulated_sd_disk_name_offset;
			*minor = *minor +
				((major - XEN_SCSI_DISK8_MAJOR + 8) * 16 * PARTS_PER_DISK) +
				emulated_sd_disk_minor_offset;
			break;
		case XENVBD_MAJOR:
			*offset = *minor / PARTS_PER_DISK;
			break;
		default:
			printk(KERN_WARNING "blkfront: your disk configuration is "
					"incorrect, please use an xvd device instead\n");
			return -ENODEV;
	}
	return 0;
}

static int xlvbd_alloc_gendisk(blkif_sector_t capacity,
				   struct blkfront_info *info,
				   u16 vdisk_info, u16 sector_size)
{
	struct gendisk *gd;
	int nr_minors = 1;
	int err;
	unsigned int offset;
	int minor;
	int nr_parts;

	BUG_ON(info->gd != NULL);
	BUG_ON(info->rq != NULL);

	if ((info->vdevice>>EXT_SHIFT) > 1) {
		/* this is above the extended range; something is wrong */
		printk(KERN_WARNING "blkfront: vdevice 0x%x is above the extended range; ignoring\n", info->vdevice);
		return -ENODEV;
	}

	if (!VDEV_IS_EXTENDED(info->vdevice)) {
		err = xen_translate_vdev(info->vdevice, &minor, &offset);
		if (err)
			return err;		
 		nr_parts = PARTS_PER_DISK;
	} else {
		minor = BLKIF_MINOR_EXT(info->vdevice);
		nr_parts = PARTS_PER_EXT_DISK;
		offset = minor / nr_parts;
		if (xen_hvm_domain() && offset < EMULATED_HD_DISK_NAME_OFFSET + 4)
			printk(KERN_WARNING "blkfront: vdevice 0x%x might conflict with "
					"emulated IDE disks,\n\t choose an xvd device name"
					"from xvde on\n", info->vdevice);
	}
	err = -ENODEV;

	if ((minor % nr_parts) == 0)
		nr_minors = nr_parts;

	err = xlbd_reserve_minors(minor, nr_minors);
	if (err)
		goto out;
	err = -ENODEV;

	gd = alloc_disk(nr_minors);
	if (gd == NULL)
		goto release;

	if (nr_minors > 1) {
		if (offset < 26)
			sprintf(gd->disk_name, "%s%c", DEV_NAME, 'a' + offset);
		else
			sprintf(gd->disk_name, "%s%c%c", DEV_NAME,
				'a' + ((offset / 26)-1), 'a' + (offset % 26));
	} else {
		if (offset < 26)
			sprintf(gd->disk_name, "%s%c%d", DEV_NAME,
				'a' + offset,
				minor & (nr_parts - 1));
		else
			sprintf(gd->disk_name, "%s%c%c%d", DEV_NAME,
				'a' + ((offset / 26) - 1),
				'a' + (offset % 26),
				minor & (nr_parts - 1));
	}

	gd->major = XENVBD_MAJOR;
	gd->first_minor = minor;
	gd->fops = &xlvbd_block_fops;
	gd->private_data = info;
	gd->driverfs_dev = &(info->xbdev->dev);
	set_capacity(gd, capacity);

	printk("Set blk queue %d\n", 
	info->max_indirect_segments ? :BLKIF_MAX_SEGMENTS_PER_REQUEST);

	if (xlvbd_init_blk_queue(gd, sector_size, 
				info->max_indirect_segments ? :
				BLKIF_MAX_SEGMENTS_PER_REQUEST)) {
		del_gendisk(gd);
		goto release;
	}

	info->rq = gd->queue;
	info->gd = gd;

#if defined(ORACLE)
	xlvbd_barrier(info);
#else
	xlvbd_flush(info);
#endif

	if (vdisk_info & VDISK_READONLY)
		set_disk_ro(gd, 1);

	if (vdisk_info & VDISK_REMOVABLE)
		gd->flags |= GENHD_FL_REMOVABLE;

	if (vdisk_info & VDISK_CDROM)
		gd->flags |= GENHD_FL_CD;

	return 0;

 release:
	xlbd_release_minors(minor, nr_minors);
 out:
	return err;
}

static void xlvbd_release_gendisk(struct blkfront_info *info)
{
	unsigned int minor, nr_minors;
	unsigned long flags;

	if (info->rq == NULL)
		return;

	spin_lock_irqsave(&blkif_io_lock, flags);

	/* No more blkif_request(). */
	blk_stop_queue(info->rq);

	/* No more gnttab callback work. */
	gnttab_cancel_free_callback(&info->callback);
	spin_unlock_irqrestore(&blkif_io_lock, flags);

	/* Flush gnttab callback work. Must be done with no locks held. */
	flush_scheduled_work();

	del_gendisk(info->gd);

	minor = info->gd->first_minor;
	nr_minors = info->gd->minors;
	xlbd_release_minors(minor, nr_minors);

	blk_cleanup_queue(info->rq);
	info->rq = NULL;

	put_disk(info->gd);
	info->gd = NULL;
}

static void kick_pending_request_queues(struct blkfront_info *info)
{
	if (!RING_FULL(&info->ring)) {
		/* Re-enable calldowns. */
		blk_start_queue(info->rq);
		/* Kick things off immediately. */
		do_blkif_request(info->rq);
	}
}

static void blkif_restart_queue(struct work_struct *work)
{
	struct blkfront_info *info = container_of(work, struct blkfront_info, work);

	spin_lock_irq(&blkif_io_lock);
	if (info->connected == BLKIF_STATE_CONNECTED)
		kick_pending_request_queues(info);
	spin_unlock_irq(&blkif_io_lock);
}

static void blkif_free(struct blkfront_info *info, int suspend)
{
	struct grant *persistent_gnt;
	struct grant *n;
	int i, j, segs;

	/* Prevent new requests being issued until we fix things up. */
	spin_lock_irq(&blkif_io_lock);
	info->connected = suspend ?
		BLKIF_STATE_SUSPENDED : BLKIF_STATE_DISCONNECTED;
	/* No more blkif_request(). */
	if (info->rq)
		blk_stop_queue(info->rq);
	

	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "persistent_gnt->pfn need not been free in suspend mode!\n");
		/* Remove all persistent grants */
		if (!list_empty(&info->grants)) {
			list_for_each_entry_safe(persistent_gnt, n,
								 &info->grants, node) {
				if (persistent_gnt->gref != GRANT_INVALID_REF) {
					gnttab_end_foreign_access(persistent_gnt->gref,
											  0, 0UL);
					info->persistent_gnts_c--;
				}
			}
		}
		BUG_ON(info->persistent_gnts_c != 0);
	}
	else
	{
		/* Remove all persistent grants */
		if (!list_empty(&info->grants)) {
			list_for_each_entry_safe(persistent_gnt, n,
								 &info->grants, node) {
				list_del(&persistent_gnt->node);
				if (persistent_gnt->gref != GRANT_INVALID_REF) {
					gnttab_end_foreign_access(persistent_gnt->gref,
											  0, 0UL);
					info->persistent_gnts_c--;
				}
				if (info->feature_persistent)
					__free_page(pfn_to_page(persistent_gnt->pfn));

				kfree(persistent_gnt);
			}
		}
		BUG_ON(info->persistent_gnts_c != 0);
	}

	/*
	 * Remove indirect pages, this only happens when using indirect
	 * descriptors but not persistent grants
	 */
	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "indirect_page need not been free in suspend mode!\n");
	}
	else
	{
		if (!list_empty(&info->indirect_pages)) {
			struct page *indirect_page, *n;

			BUG_ON(info->feature_persistent);
			list_for_each_entry_safe(indirect_page, n, &info->indirect_pages, lru) {
				list_del(&indirect_page->lru);
				__free_page(indirect_page);
			}
		}
	}

	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "list_add_tail persistent_gnt in suspend mode!\n");
		for (i = 0; i < RING_SIZE(&info->ring); i++)
		{
			if (!info->shadow[i].request)
				continue;

			segs = info->shadow[i].req.operation == BLKIF_OP_INDIRECT ?
				info->shadow[i].req.u.indirect.nr_segments :
				info->shadow[i].req.u.rw.nr_segments;

			/*256 indirect_page*/
			for (j = 0; j < segs; j++) {
				persistent_gnt = info->shadow[i].grants_used[j];
				gnttab_end_foreign_access(persistent_gnt->gref, 0, 0UL);
				list_add_tail(&persistent_gnt->node, &info->grants);
			}

			if (info->shadow[i].req.operation != BLKIF_OP_INDIRECT)
				continue;

			/*1 indirect_page_segment*/
			for (j = 0; j < INDIRECT_GREFS(segs); j++) {
				persistent_gnt = info->shadow[i].indirect_grants[j];
				gnttab_end_foreign_access(persistent_gnt->gref, 0, 0UL);
				/*indirect_page is taken from info->indirect_pages when feature_persistent is false
				   indirect_page is taken from info->grants when feature_persistent is true
				*/
				if (!info->feature_persistent) {
					struct page *indirect_page;
					indirect_page = pfn_to_page(persistent_gnt->pfn);
					printk(KERN_INFO "list_add(&indirect_page->lru, &info->indirect_pages) in suspend mode!\n");
					list_add(&indirect_page->lru, &info->indirect_pages);
				}
				list_add_tail(&persistent_gnt->node, &info->grants);
			}
		}		
	}
	else
	{
		for (i = 0; i < RING_SIZE(&info->ring); i++) {
			/*
			 * Clear persistent grants present in requests already
			 * on the shared ring
			 */
			if (!info->shadow[i].request)
				goto free_shadow;
	
			segs = info->shadow[i].req.operation == BLKIF_OP_INDIRECT ?
				info->shadow[i].req.u.indirect.nr_segments :
				info->shadow[i].req.u.rw.nr_segments;
			for (j = 0; j < segs; j++) {
				persistent_gnt = info->shadow[i].grants_used[j];
				gnttab_end_foreign_access(persistent_gnt->gref, 0, 0UL);
	
				if (info->feature_persistent)
					__free_page(pfn_to_page(persistent_gnt->pfn));
				kfree(persistent_gnt);
			}
	
			if (info->shadow[i].req.operation != BLKIF_OP_INDIRECT)
				/*
				 * If this is not an indirect operation don't try to
				 * free indirect segments
				 */
				goto free_shadow;
	
			for (j = 0; j < INDIRECT_GREFS(segs); j++) {
				persistent_gnt = info->shadow[i].indirect_grants[j];
				gnttab_end_foreign_access(persistent_gnt->gref, 0, 0UL);
				__free_page(pfn_to_page(persistent_gnt->pfn));
				kfree(persistent_gnt);
			}
free_shadow:
			kfree(info->shadow[i].grants_used);
			info->shadow[i].grants_used = NULL;
			kfree(info->shadow[i].indirect_grants);
			info->shadow[i].indirect_grants = NULL;
			kfree(info->shadow[i].sg);
			info->shadow[i].sg = NULL;
		}
	}

	/* No more gnttab callback work. */
	gnttab_cancel_free_callback(&info->callback);
	spin_unlock_irq(&blkif_io_lock);

	/* Flush gnttab callback work. Must be done with no locks held. */
	flush_scheduled_work();
	
	vunmap(info->ring.sring);
	info->ring.sring = NULL;
	/* Free resources associated with old device channel. */
	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "ring_pages need not been free in suspend mode!\n");
		for (i = 0; i < info->ring_size; i++)
		{
			if (info->ring_refs[i] != GRANT_INVALID_REF) {
				//gnttab_end_foreign_access 里面会调用free_page
				/*gnttab_end_foreign_access(info->ring_refs[i], 0, 
					(unsigned long)mfn_to_virt(pfn_to_mfn(page_to_pfn(info->ring_pages[i]))));*/
				gnttab_end_foreign_access(info->ring_refs[i], 0, 0UL);
				info->ring_refs[i] = GRANT_INVALID_REF;
			}
		}
	}
	else
	{
		for (i = 0; i < info->ring_size; i++)
		{
			if (info->ring_refs[i] != GRANT_INVALID_REF) {
				//gnttab_end_foreign_access 里面会调用free_page,所以不用显式在调用__free_page(info->ring_pages[i]);
				gnttab_end_foreign_access(info->ring_refs[i], 0, 
					(unsigned long)mfn_to_virt(pfn_to_mfn(page_to_pfn(info->ring_pages[i]))));
				info->ring_refs[i] = GRANT_INVALID_REF;
			}
			//if(info->ring_pages_copy[i])
			//	__free_page(info->ring_pages_copy[i]);
		}
	}
	
	if (info->irq)
		unbind_from_irqhandler(info->irq, info);
	info->evtchn = info->irq = 0;
	
}

static void blkif_completion(struct blk_shadow *s, struct blkfront_info *info,
			     struct blkif_response *bret)
{
	int i = 0;
	int nseg;
	/* for persistent grant */
	struct scatterlist *sg;
	char *bvec_data;
	void *shared_data;

	nseg = s->req.operation == BLKIF_OP_INDIRECT ?
		s->req.u.indirect.nr_segments : s->req.u.rw.nr_segments;

	if (bret->operation == BLKIF_OP_READ && info->feature_persistent) {
		/*
		 * Copy the data received from the backend into the bvec.
		 * Since bv_offset can be different than 0, and bv_len different
		 * than PAGE_SIZE, we have to keep track of the current offset,
		 * to be sure we are copying the data from the right shared page.
		 */
		for_each_sg(s->sg, sg, nseg, i) {
			BUG_ON(sg->offset + sg->length > PAGE_SIZE);
			shared_data = kmap_atomic(
				pfn_to_page(s->grants_used[i]->pfn), KM_USER0);
			if (in_interrupt()) {
				if (in_irq())
					bvec_data = kmap_atomic(sg_page(sg), KM_IRQ0);
				else if (in_softirq())
					bvec_data = kmap_atomic(sg_page(sg), KM_SOFTIRQ0);
				else
					printk(KERN_WARNING "[kmap] in interrupt, but not irq!\n");
			}
			else
				bvec_data = kmap_atomic(sg_page(sg), KM_USER0);

			memcpy(bvec_data   + sg->offset,
			       shared_data + sg->offset,
			       sg->length);
			if (in_interrupt()) {
				if (in_irq())
					kunmap_atomic(bvec_data, KM_IRQ0);
				else if (in_softirq())
					kunmap_atomic(bvec_data, KM_SOFTIRQ0);
				else
					printk(KERN_WARNING "[kunmap] in interrupt, but not irq!\n");
			}
			else
				kunmap_atomic(bvec_data, KM_USER0);
			kunmap_atomic(shared_data, KM_USER0);
		}
	}
	/* Add the persistent grant into the list of free grants */
	for (i = 0; i < nseg; i++) {
		if (gnttab_query_foreign_access(s->grants_used[i]->gref)) {
			/*
			 * If the grant is still mapped by the backend (the
			 * backend has chosen to make this grant persistent)
			 * we add it at the head of the list, so it will be
			 * reused first.
			 */
			if (!info->feature_persistent)
				printk(KERN_WARNING "backed has not unmapped grant: %u\n",
						     s->grants_used[i]->gref);
			list_add(&s->grants_used[i]->node, &info->grants);
			info->persistent_gnts_c++;
		} else {
			/*
			 * If the grant is not mapped by the backend we end the
			 * foreign access and add it to the tail of the list,
			 * so it will not be picked again unless we run out of
			 * persistent grants.
			 */
			gnttab_end_foreign_access(s->grants_used[i]->gref, 0, 0UL);
			s->grants_used[i]->gref = GRANT_INVALID_REF;
			list_add_tail(&s->grants_used[i]->node, &info->grants);
		}
	}
	if (s->req.operation == BLKIF_OP_INDIRECT) {
		for (i = 0; i < INDIRECT_GREFS(nseg); i++) {
			if (gnttab_query_foreign_access(s->indirect_grants[i]->gref)) {
				if (!info->feature_persistent)
					printk(KERN_WARNING "backed has not unmapped grant: %u\n",
							s->indirect_grants[i]->gref);
				list_add(&s->indirect_grants[i]->node, &info->grants);
				info->persistent_gnts_c++;
			} else {
				struct page *indirect_page;

				gnttab_end_foreign_access(s->indirect_grants[i]->gref, 0, 0UL);
				/*
				 * Add the used indirect page back to the list of
				 * available pages for indirect grefs.
				 */
				if (!info->feature_persistent) {
					indirect_page = pfn_to_page(s->indirect_grants[i]->pfn);
					list_add(&indirect_page->lru, &info->indirect_pages);
				}
				s->indirect_grants[i]->gref = GRANT_INVALID_REF;
				list_add_tail(&s->indirect_grants[i]->node, &info->grants);
			}
		}
	}
}

static irqreturn_t blkif_interrupt(int irq, void *dev_id)
{
	struct request *req;
	struct blkif_response *bret;
	RING_IDX i, rp;
	unsigned long flags;
	struct blkfront_info *info = (struct blkfront_info *)dev_id;
	int error;

	spin_lock_irqsave(&blkif_io_lock, flags);

	if (unlikely(info->connected != BLKIF_STATE_CONNECTED)) {
		spin_unlock_irqrestore(&blkif_io_lock, flags);
		return IRQ_HANDLED;
	}

 again:
	rp = info->ring.sring->rsp_prod;
	rmb(); /* Ensure we see queued responses up to 'rp'. */

	for (i = info->ring.rsp_cons; i != rp; i++) {
		unsigned long id;

		bret = RING_GET_RESPONSE(&info->ring, i);
		id   = bret->id;
		req  = info->shadow[id].request;

		blkif_completion(&info->shadow[id], info, bret);

		add_id_to_freelist(info, id);

		error = (bret->status == BLKIF_RSP_OKAY) ? 0 : -EIO;
		switch (bret->operation) {
		case BLKIF_OP_WRITE_BARRIER:
			if (unlikely(bret->status == BLKIF_RSP_EOPNOTSUPP)) {
				printk(KERN_WARNING "blkfront: %s: write barrier op failed\n",
					   info->gd->disk_name);
				error = -EOPNOTSUPP;
				info->feature_flush = 0;
#if defined(ORACLE)
				xlvbd_barrier(info);
#else
				xlvbd_flush(info);
#endif
			}
			/* fall through */
		case BLKIF_OP_READ:
		case BLKIF_OP_WRITE:
			if (unlikely(bret->status != BLKIF_RSP_OKAY))
				dev_dbg(&info->xbdev->dev, "Bad return from blkdev data "
					"request: %x\n", bret->status);

			__blk_end_request_all(req, error);
			break;
		default:
			BUG();
		}
	}

	info->ring.rsp_cons = i;

	if (i != info->ring.req_prod_pvt) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_RESPONSES(&info->ring, more_to_do);
		if (more_to_do)
			goto again;
	} else
		info->ring.sring->rsp_event = i + 1;

	kick_pending_request_queues(info);

	spin_unlock_irqrestore(&blkif_io_lock, flags);

	return IRQ_HANDLED;
}


static int setup_blkring(struct xenbus_device *dev,
			 struct blkfront_info *info)
{
	struct blkif_sring *sring;
	int err;
	unsigned int nr;
	int i;

	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "ring_pages need not been realloc_page in suspend mode!\n");
		//printk(KERN_INFO "info->ring_pages = info->ring_pages_copy in suspend mode!\n");
		for (nr = 0; nr < info->ring_size; nr++) {
			info->ring_refs[nr] = GRANT_INVALID_REF;
			//info->ring_pages[nr] = info->ring_pages_copy[nr];
		}
	}
	else
	{
		for (nr = 0; nr < info->ring_size; nr++) {
			info->ring_refs[nr] = GRANT_INVALID_REF;
			info->ring_pages[nr] = alloc_page(GFP_NOIO | __GFP_HIGH);
			if (!info->ring_pages[nr])
			{
				break;
			}
		}
	}
	
	sring = (nr == info->ring_size)
		? vmap(info->ring_pages, nr, VM_MAP, PAGE_KERNEL)
		: NULL;
	if (!sring) {
		while (nr--)
			__free_page(info->ring_pages[nr]);
		xenbus_dev_fatal(dev, -ENOMEM, "allocating shared ring");
		return -ENOMEM;
	}
	SHARED_RING_INIT(sring);
	FRONT_RING_INIT(&info->ring, sring,
			(unsigned long)info->ring_size << PAGE_SHIFT);
	for (i=0; i<info->ring_size; i++) {
		err = xenbus_grant_ring(dev, pfn_to_mfn(page_to_pfn(info->ring_pages[i])));
		if (err < 0)
		{
			printk(KERN_ERR "failed to xenbus_grant_ring!\n");
			goto fail;
		}
		info->ring_refs[i] = err;
	}

	err = xenbus_alloc_evtchn(dev, &info->evtchn);
	if (err)
		goto fail;

	err = bind_evtchn_to_irqhandler(info->evtchn,
					blkif_interrupt,
					IRQF_SAMPLE_RANDOM, "blkif", info);
	if (err <= 0) {
		xenbus_dev_fatal(dev, err,
				 "bind_evtchn_to_irqhandler failed");
		goto fail;
	}
	info->irq = err;

	return 0;
fail:
	blkif_free(info, 0);
	return err;
}

static void shadow_init(struct blk_shadow *shadow, unsigned int ring_size)
{
	unsigned int i = 0;

	WARN_ON(!ring_size);
	while (++i < ring_size)
		shadow[i - 1].req.u.rw.id = i;
	shadow[i - 1].req.u.rw.id = 0x0fffffff;
}

/* Common code used when first setting up, and when resuming. */
static int talk_to_blkback(struct xenbus_device *dev,
			   struct blkfront_info *info)
{
	unsigned int ring_size, ring_order;
	const char *what = NULL;
	struct xenbus_transaction xbt;
	int err;
	int nr = 0;

	if (dev->state >= XenbusStateInitialised)
		return 0;

	err = xenbus_scanf(XBT_NIL, dev->otherend,
			   "max-ring-pages", "%u", &ring_size);
	if (err != 1)
		ring_size = 0;
	else if (!ring_size)
		pr_warn("blkfront: %s: zero max-ring-pages\n", dev->nodename);

	err = xenbus_scanf(XBT_NIL, dev->otherend,
			   "max-ring-page-order", "%u", &ring_order);
	if (err != 1)
		ring_order = ring_size ? ilog2(ring_size) : 0;
	else if (!ring_size)
		/* nothing */;
	else if ((ring_size - 1) >> ring_order)
		pr_warn("blkfront: %s: max-ring-pages (%#x) inconsistent with"
			" max-ring-page-order (%u)\n",
			dev->nodename, ring_size, ring_order);
	else
		ring_order = ilog2(ring_size);

	if (ring_order > BLK_MAX_RING_PAGE_ORDER)
		ring_order = BLK_MAX_RING_PAGE_ORDER;
	/*
	 * While for larger rings not all pages are actually used, be on the
	 * safe side and set up a full power of two to please as many backends
	 * as possible.
	 */
	printk("talk_to_blkback ring_size %d, ring_order %d\n", 1U<<ring_order, ring_order);
	info->ring_size = ring_size = 1U << ring_order;

	/* Create shared ring, alloc event channel. */
	err = setup_blkring(dev, info);
	if (err)
		goto out;

again:
	err = xenbus_transaction_start(&xbt);
	if (err) {
		xenbus_dev_fatal(dev, err, "starting transaction");
		goto destroy_blkring;
	}

	if (ring_size == 1) {
		what = "ring-ref";
		err = xenbus_printf(xbt, dev->nodename,
					what, "%u", info->ring_refs[0]);
		if (err) {
			goto abort_transaction;
		}
	} else {
		unsigned int i;
		char buf[16];

		what = "ring-page-order";
		err = xenbus_printf(xbt, dev->nodename, what, "%u",
					ring_order);
		if (err)
			goto abort_transaction;
		what = "num-ring-pages";
		err = xenbus_printf(xbt, dev->nodename, what, "%u", ring_size);
		if (err)
			goto abort_transaction;
		what = buf;
		for (i = 0; i < ring_size; i++) {
			snprintf(buf, sizeof(buf), "ring-ref%u", i);
			err = xenbus_printf(xbt, dev->nodename, what, "%u",
						info->ring_refs[i]);
			if (err)
				goto abort_transaction;
		}
	}

	what = "event-channel";
	err = xenbus_printf(xbt, dev->nodename,
				what, "%u", info->evtchn);
	if (err) {
		goto abort_transaction;
	}
	what = "protocol";
	err = xenbus_printf(xbt, dev->nodename, what, "%s",
				XEN_IO_PROTO_ABI_NATIVE);
	if (err) {
		goto abort_transaction;
	}

	/* for persistent grant */
	err = xenbus_printf(xbt, dev->nodename,
			    "feature-persistent", "%u", 1);
	if (err)
		dev_warn(&dev->dev,
			 "writing persistent grants feature to xenbus");

	err = xenbus_transaction_end(xbt, 0);
	if (err) {
		if (err == -EAGAIN)
			goto again;
		xenbus_dev_fatal(dev, err, "completing transaction");
		goto destroy_blkring;
	}

	xenbus_switch_state(dev, XenbusStateInitialised);

	ring_size = RING_SIZE(&info->ring);
	switch (info->connected) {
	case BLKIF_STATE_DISCONNECTED:
		shadow_init(info->shadow, ring_size);
		break;
	case BLKIF_STATE_SUSPENDED:
		err = blkif_recover(info);
		if (err)
			goto out;
		break;
	}

	#if 0
	/*realloc_page ring_pages_copy for next hot-migrate after disk has resumed*/
	for (nr = 0; nr < info->ring_size; nr++) {
		info->ring_pages_copy[nr] = alloc_page(GFP_NOIO | __GFP_HIGH);
		if (!info->ring_pages_copy[nr])
		{
			printk(KERN_ERR "failed to alloc_page ring_pages_copy!\n");
			err = -ENOMEM;
			goto destroy_blkring;
		}
	}
	#endif
		
	pr_info("blkfront: %s: ring-pages=%u nr_ents=%u\n",
		dev->nodename, info->ring_size, ring_size);
	return 0;

 abort_transaction:
	xenbus_transaction_end(xbt, 1);
	if (what)
		xenbus_dev_fatal(dev, err, "writing %s", what);
 destroy_blkring:
	blkif_free(info, 0);
 out:
	return err;
}

/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and the ring buffer for communication with the backend, and
 * inform the backend of the appropriate details for those.  Switch to
 * Initialised state.
 */
static int blkfront_probe(struct xenbus_device *dev,
			  const struct xenbus_device_id *id)
{
	int err, vdevice;
	struct blkfront_info *info;
	enum xenbus_state backend_state;

	/* FIXME: Use dynamic device id if this is not set. */
	err = xenbus_scanf(XBT_NIL, dev->nodename,
			   "virtual-device", "%i", &vdevice);
	if (err != 1) {
		/* go looking in the extended area instead */
		err = xenbus_scanf(XBT_NIL, dev->nodename, "virtual-device-ext",
				   "%i", &vdevice);
		if (err != 1) {
			xenbus_dev_fatal(dev, err, "reading virtual-device");
			return err;
		}
	}

	/* 	
	 * Prevent hooking up IDE if ide-unplug not supported;
	 * Never hook up SCSI devices on pv-on-hvm guest
	 */
	if (xen_hvm_domain()) {
		extern int xen_ide_unplug_unsupported;
		int major;
		int cfg = 1;
		char* type;
		int len;

		if (!VDEV_IS_EXTENDED(vdevice))
			major = BLKIF_MAJOR(vdevice);
		else
			major = XENVBD_MAJOR;

		switch(major) {
		case IDE0_MAJOR:
		case IDE1_MAJOR:
		case IDE2_MAJOR:
		case IDE3_MAJOR:
		case IDE4_MAJOR:
		case IDE5_MAJOR:
		case IDE6_MAJOR:
		case IDE7_MAJOR:
		case IDE8_MAJOR:
		case IDE9_MAJOR:
			if (xen_ide_unplug_unsupported)
				cfg = 0;
			break;
		case SCSI_DISK0_MAJOR:
		case SCSI_DISK1_MAJOR:
		case SCSI_DISK2_MAJOR:
		case SCSI_DISK3_MAJOR:
		case SCSI_DISK4_MAJOR:
		case SCSI_DISK5_MAJOR:
		case SCSI_DISK6_MAJOR:
		case SCSI_DISK7_MAJOR:
		case SCSI_DISK8_MAJOR:
		case SCSI_DISK9_MAJOR:
		case SCSI_DISK10_MAJOR:
		case SCSI_DISK11_MAJOR:
		case SCSI_DISK12_MAJOR:
		case SCSI_DISK13_MAJOR:
		case SCSI_DISK14_MAJOR:
		case SCSI_DISK15_MAJOR:
			cfg = 0;
			break;
		}
		if (cfg == 0) {
			printk(KERN_INFO
				"%s: HVM does not support vbd %d as xen block device\n",
			__FUNCTION__, vdevice);
			return -ENODEV;
		}

		/* do not create a PV cdrom device if we are an HVM guest */
		type = xenbus_read(XBT_NIL, dev->nodename, "device-type", &len);
		if (IS_ERR(type))
			return -ENODEV;
		if (strncmp(type, "cdrom", 5) == 0) {
			kfree(type);
			return -ENODEV;
		}
		kfree(type);
	}


	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		xenbus_dev_fatal(dev, -ENOMEM, "allocating info structure");
		return -ENOMEM;
	}

	mutex_init(&info->mutex);
	info->xbdev = dev;
	info->vdevice = vdevice;
	INIT_LIST_HEAD(&info->grants);
	INIT_LIST_HEAD(&info->indirect_pages);
	/* for persistent grant */
	info->persistent_gnts_c = 0;
	info->connected = BLKIF_STATE_DISCONNECTED;
	//add by chijc
	info->blkif_detach = 0;
	info->users = 0;
	INIT_WORK(&info->work, blkif_restart_queue);

	/* Front end dir is a number, which is used as the id. */
	info->handle = simple_strtoul(strrchr(dev->nodename, '/')+1, NULL, 0);
	dev_set_drvdata(&dev->dev, info);

	backend_state = xenbus_read_driver_state(dev->otherend);
	/*
	 * XenbusStateInitWait would be the correct state to enter here,
	 * but (at least) blkback considers this a fatal error.
	 */
	xenbus_switch_state(dev, XenbusStateInitialising);
	if (backend_state != XenbusStateInitWait)
		return 0;

	err = talk_to_blkback(dev, info);
	if (err) {
		kfree(info);
		dev_set_drvdata(&dev->dev, NULL);
		return err;
	}

	return 0;
}

/*
 * This is a clone of md_trim_bio, used to split a bio into smaller ones
 */
static void trim_bio(struct bio *bio, int offset, int size)
{
	/* 'bio' is a cloned bio which we need to trim to match
	* the given offset and size.
	* This requires adjusting bi_sector, bi_size, and bi_io_vec
	*/
	int i;
	struct bio_vec *bvec;
	int sofar = 0;

	size <<= 9;
	if (offset == 0 && size == bio->bi_size)
		return;

	bio->bi_sector += offset;
	bio->bi_size = size;
	offset <<= 9;
	clear_bit(BIO_SEG_VALID, &bio->bi_flags);

	while (bio->bi_idx < bio->bi_vcnt &&
		bio->bi_io_vec[bio->bi_idx].bv_len <= offset) {
		/* remove this whole bio_vec */
		offset -= bio->bi_io_vec[bio->bi_idx].bv_len;
		bio->bi_idx++;
	}
	if (bio->bi_idx < bio->bi_vcnt) {
		bio->bi_io_vec[bio->bi_idx].bv_offset += offset;
		bio->bi_io_vec[bio->bi_idx].bv_len -= offset;
	}
	/* avoid any complications with bi_idx being non-zero*/
	if (bio->bi_idx) {
		memmove(bio->bi_io_vec, bio->bi_io_vec+bio->bi_idx,
				(bio->bi_vcnt - bio->bi_idx) * sizeof(struct bio_vec));
		bio->bi_vcnt -= bio->bi_idx;
		bio->bi_idx = 0;
	}
	/* Make sure vcnt and last bv are not too big */
	bio_for_each_segment(bvec, bio, i) {
		if (sofar + bvec->bv_len > size)
			bvec->bv_len = size - sofar;
		if (bvec->bv_len == 0) {
			bio->bi_vcnt = i;
			break;
		}
		sofar += bvec->bv_len;
	}
}

static void split_bio_end(struct bio *bio, int error)
{
	struct split_bio *split_bio = bio->bi_private;

	if (error)
		split_bio->err = error;

	if (atomic_dec_and_test(&split_bio->pending)) {
		split_bio->bio->bi_phys_segments = 0;
		bio_endio(split_bio->bio, split_bio->err);
		kfree(split_bio);
	}
	bio_put(bio);
}

static int blkif_recover(struct blkfront_info *info)
{
	int i;
	struct request *req, *n;
	int rc;
	struct bio *bio, *cloned_bio;
	struct bio_list bio_list, merge_bio;
	unsigned int segs, offset;
	int pending, size;
	struct split_bio *split_bio;
	struct list_head requests;
	
    memcpy(info->shadow_copy, info->shadow, sizeof(info->shadow));
	/* Stage 2: Set up free list. */
	//memset(&info->shadow, 0, sizeof(info->shadow));
	for(i = 0;i < RING_SIZE(&info->ring);i++)
	{
		memset(&info->shadow[i].req,0,sizeof(struct blkif_request));
		info->shadow[i].request = NULL;
	}
	shadow_init(info->shadow, RING_SIZE(&info->ring));
	info->shadow_free = info->ring.req_prod_pvt;

	rc = blkfront_setup_indirect(info);
	if (rc) {
		return rc;
	}

	segs = info->max_indirect_segments ? : BLKIF_MAX_SEGMENTS_PER_REQUEST;
#if defined(ORACLE)
	blk_queue_max_phys_segments(info->rq, segs);
	blk_queue_max_hw_segments(info->rq, segs);
#else
	blk_queue_max_segments(info->rq, segs);
#endif

	bio_list_init(&bio_list);
	INIT_LIST_HEAD(&requests);

	/* Stage 3: Find pending requests and requeue them. */
	for (i = 0; i < RING_SIZE(&info->ring); i++) {
		/* Not in use? */
		if (!info->shadow_copy[i].request)
			continue;

		merge_bio.head = info->shadow_copy[i].request->bio;
		merge_bio.tail = info->shadow_copy[i].request->biotail;
		bio_list_merge(&bio_list, &merge_bio);
		info->shadow_copy[i].request->bio = NULL;
        blk_end_request_all(info->shadow_copy[i].request, 0);
	}

	/*
	 * Empty the queue, this is important because we might have
	 * requests in the queue with more segments than what we
	 * can handle now.
	 */
	spin_lock_irq(&blkif_io_lock);
	while ((req = blk_fetch_request(info->rq)) != NULL) {
		merge_bio.head = req->bio;
		merge_bio.tail = req->biotail;
		bio_list_merge(&bio_list, &merge_bio);
		req->bio = NULL;
#ifndef ORACLE
		if (req->cmd_flags & (REQ_FLUSH | REQ_FUA))
			pr_alert("diskcache flush request found!\n");
#endif
		//__blk_put_request(info->rq, req);
        __blk_end_request_all(req, 0);

	}
	spin_unlock_irq(&blkif_io_lock);

	xenbus_switch_state(info->xbdev, XenbusStateConnected);

	spin_lock_irq(&blkif_io_lock);

	/* Now safe for us to use the shared ring */
	info->connected = BLKIF_STATE_CONNECTED;

	/* Send off requeued requests */
	//flush_requests(info);

	/* Kick any other new requests queued since we resumed */
	kick_pending_request_queues(info);

	list_for_each_entry_safe(req, n, &requests, queuelist) {
		/* Requeue pending requests (flush or discard) */
		list_del_init(&req->queuelist);
		BUG_ON(req->nr_phys_segments > segs);
		blk_requeue_request(info->rq, req);
	}

	spin_unlock_irq(&blkif_io_lock);

	while ((bio = bio_list_pop(&bio_list)) != NULL) {
		/* Traverse the list of pending bios and re-queue them */
		if (bio_segments(bio) > segs) {
			/*
			 * This bio has more segments than what we can
			 * handle, we have to split it.
			 */
			pending = (bio_segments(bio) + segs - 1) / segs;
			split_bio = kzalloc(sizeof(*split_bio), GFP_NOIO);
			BUG_ON(split_bio == NULL);
			atomic_set(&split_bio->pending, pending);
			split_bio->bio = bio;
			for (i = 0; i < pending; i++) {
				offset = (i * segs * PAGE_SIZE) >> 9;
				size = min((unsigned int)(segs * PAGE_SIZE) >> 9,
					   (unsigned int)(bio->bi_size >> 9) - offset);
				cloned_bio = bio_clone(bio, GFP_NOIO);
				BUG_ON(cloned_bio == NULL);
				trim_bio(cloned_bio, offset, size);
				cloned_bio->bi_private = split_bio;
				cloned_bio->bi_end_io = split_bio_end;
				submit_bio(cloned_bio->bi_rw, cloned_bio);
			}
			/*
			 * Now we have to wait for all those smaller bios to
			 * end, so we can also end the "parent" bio.
			 */
			continue;
		}
		/* We don't need to split this bio */
		submit_bio(bio->bi_rw, bio);
	}
	
	return 0;
}

#define SWAP_MODIFY 1
#define SWAP_RESTORE 2
static void modify_swappiness(int mode)
{
	int ret = 0;
	static char *envp[] = { "HOME=/", "TERM=linux",
				"PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL };
	static char *modify_argv[] = { "/bin/sh", "/etc/.uvp-monitor/modify_swappiness.sh", "modify", NULL };
	static char *restore_argv[] = { "/bin/sh", "/etc/.uvp-monitor/modify_swappiness.sh", "restore", NULL };

	if(mode == SWAP_MODIFY)
	{
		ret = call_usermodehelper("/bin/sh", modify_argv, envp, UMH_WAIT_PROC);
		if (ret < 0)
		{
			printk(KERN_ERR "Failed to modify_swappiness in SWAP_MODIFY mode,ret(%d).\n",ret);
			return;
		}
	}
	else if(mode == SWAP_RESTORE)
	{
		ret = call_usermodehelper("/bin/sh", restore_argv, envp, UMH_WAIT_PROC);
		if (ret < 0)
		{
			printk(KERN_ERR "Failed to modify_swappiness in SWAP_RESTORE mode,ret(%d).\n",ret);
			return;
		}
	}
	
	return;
}

static int blkfront_suspend(struct xenbus_device *dev, pm_message_t state)
{
	modify_swappiness(SWAP_MODIFY);

	return 0;
}

/**
 * We are reconnecting to the backend, due to a suspend/resume, or a backend
 * driver restart.  We tear down our blkif structure and recreate it, but
 * leave the device-layer structures intact so that this is transparent to the
 * rest of the kernel.
 */
static int blkfront_resume(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev_get_drvdata(&dev->dev);
	int err = 0;
	enum xenbus_state backend_state;
	int i;

	for (i=0; i<info->ring_size; i++) {
		if (gnttab_query_foreign_access(info->ring_refs[i]))
		{
			printk(KERN_ERR "gnttab_query_foreign_access is true,blkfront_resume is break!\n");
			goto restore_swap;
		}
	}
	
	dev_dbg(&dev->dev, "blkfront_resume: %s\n", dev->nodename);

	blkif_free(info, info->connected == BLKIF_STATE_CONNECTED);

	backend_state = xenbus_read_driver_state(dev->otherend);
	/* See respective comment in blkfront_probe(). */
	xenbus_switch_state(dev, XenbusStateInitialising);
	if (backend_state != XenbusStateInitWait)
		goto restore_swap;
	
	err = talk_to_blkback(dev, info);

restore_swap:
	modify_swappiness(SWAP_RESTORE);
	/*
	 * We have to wait for the backend to switch to
	 * connected state, since we want to read which
	 * features it supports.
	 */

	return err;
}

static void
blkfront_closing(struct blkfront_info *info)
{
	struct xenbus_device *xbdev = info->xbdev;
	struct block_device *bdev = NULL;
	int len;
	char *devname;
	char *bestate;
	int iTimeout = 0;

	devname = xenbus_read(XBT_NIL, xbdev->otherend, "dev", &len);
	if (IS_ERR(devname))
	{
		printk(KERN_ERR "read %s xenstore error!\n", xbdev->otherend);
	}
	else{
		xenbus_write(XBT_NIL, "control/uvp", "unplug-disk", devname);
		kfree(devname);
	}
	while(info && info->users != 0)
	{
		printk(KERN_INFO "info->users == %d wait %d!\n", info->users, iTimeout);
		if(iTimeout > UNPLUG_DISK_TMOUT)
			break;
		ssleep(1);
		iTimeout++;
	}

	bestate = xenbus_read(XBT_NIL, xbdev->otherend, "state", &len);
	if (IS_ERR(bestate)){
		printk(KERN_ERR "read %s state error!\n", xbdev->otherend);
	}
	else{
		if(strncmp(bestate, "5", 1) || iTimeout > UNPLUG_DISK_TMOUT) {
			info->blkif_detach = 0;
			kfree(bestate);
			return;
		}
		kfree(bestate);
	}
	
	mutex_lock(&info->mutex);

	if (xbdev->state == XenbusStateClosing) {
		mutex_unlock(&info->mutex);
		return;
	}

	if (info->gd)
		bdev = bdget_disk(info->gd, 0);

	mutex_unlock(&info->mutex);

	if (!bdev) {
		xenbus_frontend_closed(xbdev);
		return;
	}

	mutex_lock(&bdev->bd_mutex);

	if (bdev->bd_openers) {
		xenbus_dev_error(xbdev, -EBUSY,
				 "Device in use; refusing to close");
	} else {
		xlvbd_release_gendisk(info);
		xenbus_frontend_closed(xbdev);
	}

	mutex_unlock(&bdev->bd_mutex);
	bdput(bdev);
}

static int blkfront_setup_indirect(struct blkfront_info *info)
{
	unsigned int indirect_segments, segs;
	int err, i;
	struct grant *persistent_gnt;
	struct grant *n;
	unsigned int grants_nums = 0;

	//for debug
	unsigned int cmp_grants_nums = 0;
	unsigned int cmp_indirect_pages_nums = 0;

	info->max_indirect_segments = 0;
	segs = BLKIF_MAX_SEGMENTS_PER_REQUEST;

	err = xenbus_gather(XBT_NIL, info->xbdev->otherend,
		"feature-max-indirect-segments", "%u", &indirect_segments,
		NULL);

	if (!err && indirect_segments > BLKIF_MAX_SEGMENTS_PER_REQUEST) {
		info->max_indirect_segments = min(indirect_segments,
			xen_blkif_max_segments);
		segs = info->max_indirect_segments;
	}

	printk("[%s:%d], segs %d\n", __func__, __LINE__, segs);
	grants_nums = (segs + INDIRECT_GREFS(segs)) * RING_SIZE(&info->ring);
	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "granted_page(persistent_feature) need not been realloc_page in suspend mode!\n");
		if (info->feature_persistent) {
			if (!list_empty(&info->grants)) {
				list_for_each_entry_safe(persistent_gnt, n,
									 &info->grants, node) {
					persistent_gnt->gref = GRANT_INVALID_REF;
					cmp_grants_nums++;
				}
				printk(KERN_INFO "grants_nums should be %d,cmp_grants_nums is %d in suspend mode when persistent_feature is true!\n",
									grants_nums, cmp_grants_nums);
			}
		}
		else
		{
			if (!list_empty(&info->grants)) {
				list_for_each_entry_safe(persistent_gnt, n,
									 &info->grants, node) {
					persistent_gnt->gref = GRANT_INVALID_REF;
					persistent_gnt->pfn = 0;
					cmp_grants_nums++;
				}
				printk(KERN_INFO "grants_nums should be %d,cmp_grants_nums is %d in suspend mode when persistent_feature is false!\n",
									grants_nums, cmp_grants_nums);
			}
		}
	}
	else
	{
		err = fill_grant_buffer(info, grants_nums);
		if (err)
			goto out_of_memory;
	}

	if (!info->feature_persistent && info->max_indirect_segments) {
		/*
		 * We are using indirect descriptors but not persistent
		 * grants, we need to allocate a set of pages that can be
		 * used for mapping indirect grefs
		 */
		int indirect_pages_nums = INDIRECT_GREFS(segs) * RING_SIZE(&info->ring);
		if(info->connected == BLKIF_STATE_SUSPENDED)
		{
			printk(KERN_INFO "indirect_page need not been realloc_page in suspend mode!\n");
			if (!list_empty(&info->indirect_pages)) {
				struct page *indirect_page, *n;

				BUG_ON(info->feature_persistent);
				list_for_each_entry_safe(indirect_page, n, &info->indirect_pages, lru) {
					cmp_indirect_pages_nums++;
				}
				
				printk(KERN_INFO "indirect_pages_nums should be %d,cmp_indirect_pages_nums is %d in suspend mode!\n", 
						indirect_pages_nums, cmp_indirect_pages_nums);
			}
		}
		else
		{
			BUG_ON(!list_empty(&info->indirect_pages));
			for (i = 0; i < indirect_pages_nums; i++) {
				struct page *indirect_page = alloc_page(GFP_NOIO);
				if (!indirect_page)
					goto out_of_memory;
				list_add(&indirect_page->lru, &info->indirect_pages);
			}
		}
	}

	if(info->connected == BLKIF_STATE_SUSPENDED)
	{
		printk(KERN_INFO "grants_used,sg and indirect_grants need not been rekzalloc in suspend mode!\n");
		for (i = 0; i < RING_SIZE(&info->ring); i++) {
			memset(info->shadow[i].grants_used, 0, sizeof(info->shadow[i].grants_used[0]) * segs);
			memset(info->shadow[i].sg, 0, sizeof(info->shadow[i].sg[0]) * segs);
			if (info->max_indirect_segments)
				memset(info->shadow[i].indirect_grants, 0, 
						sizeof(info->shadow[i].indirect_grants[0]) * INDIRECT_GREFS(segs));
			/* initialise every shadow's sg */
			sg_init_table(info->shadow[i].sg, segs);
		}
	}
	else
	{
		for (i = 0; i < RING_SIZE(&info->ring); i++) {
			info->shadow[i].grants_used = kzalloc(
				sizeof(info->shadow[i].grants_used[0]) * segs,
				GFP_NOIO);
			/* malloc space for every shadow's sg */
			info->shadow[i].sg = kzalloc(sizeof(info->shadow[i].sg[0]) * segs, GFP_NOIO);
			if (info->max_indirect_segments)
				info->shadow[i].indirect_grants = kzalloc(
					sizeof(info->shadow[i].indirect_grants[0]) *
					INDIRECT_GREFS(segs),
					GFP_NOIO);
			if ((info->shadow[i].grants_used == NULL) ||
				(info->shadow[i].sg == NULL) ||
				(info->max_indirect_segments &&
				(info->shadow[i].indirect_grants == NULL)))
				goto out_of_memory;
			/* initialise every shadow's sg */
			sg_init_table(info->shadow[i].sg, segs);
		}
	}

	return 0;

out_of_memory:
	for (i = 0; i < RING_SIZE(&info->ring); i++) {
		kfree(info->shadow[i].grants_used);
		info->shadow[i].grants_used = NULL;
		/* free every shadow's sg */
		kfree(info->shadow[i].sg);
		info->shadow[i].sg = NULL;
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

/*
 * Invoked when the backend is finally 'ready' (and has told produced
 * the details about the physical device - #sectors, size, etc).
 */
static void blkfront_connect(struct blkfront_info *info)
{
	unsigned long long sectors;
	unsigned long sector_size;
	unsigned int binfo;
	int err;
	int barrier;
	dev_t devt;
	/* for persistent grant */
	int persistent;

	switch (info->connected) {
	case BLKIF_STATE_CONNECTED:
		/*
		 * Potentially, the back-end may be signalling
		 * a capacity change; update the capacity.
		 */
		err = xenbus_scanf(XBT_NIL, info->xbdev->otherend,
				   "sectors", "%Lu", &sectors);
		if (XENBUS_EXIST_ERR(err))
			return;

		devt = disk_devt(info->gd);
		printk(KERN_INFO "Changing capacity of (%u, %u) to %Lu "
			   "sectors\n", (unsigned)MAJOR(devt),
			   (unsigned)MINOR(devt), sectors);

		set_capacity(info->gd, sectors);
		revalidate_disk(info->gd);

		return;
	case BLKIF_STATE_SUSPENDED:
		/*
		 * If we are recovering from suspension, we need to wait
		 * for the backend to announce it's features before
		 * reconnecting, at least we need to know if the backend
		 * supports indirect descriptors, and how many.
		 */
		blkif_recover(info);
		return;

	default:
		/* keep gcc quiet; ISO C99 6.8.4.2p5, 6.8.3p6 */
		;
	}

	dev_dbg(&info->xbdev->dev, "%s:%s.\n",
		__func__, info->xbdev->otherend);

	err = xenbus_gather(XBT_NIL, info->xbdev->otherend,
				"sectors", "%llu", &sectors,
				"info", "%u", &binfo,
				"sector-size", "%lu", &sector_size,
				NULL);
	if (err) {
		xenbus_dev_fatal(info->xbdev, err,
				 "reading backend fields at %s",
				 info->xbdev->otherend);
		return;
	}

	err = xenbus_gather(XBT_NIL, info->xbdev->otherend,
				"feature-barrier", "%d", &barrier,
				NULL);

	/*
	 * If there's no "feature-barrier" defined, then it means
	 * we're dealing with a very old backend which writes
	 * synchronously; nothing to do.
	 *
	 * If there are barriers, then we use flush.
	 */
	info->feature_flush = 0;

	/*
	 * The driver doesn't properly handled empty flushes, so
	 * lets disable barrier support for now.
	 */
#if 0
	if (!err && barrier)
		info->feature_flush = REQ_FLUSH;
#endif

	err = xenbus_gather(XBT_NIL, info->xbdev->otherend,
			    "feature-persistent", "%u", &persistent,
			    NULL);
	if (err)
		info->feature_persistent = 0;
	else
		info->feature_persistent = persistent;

	err = blkfront_setup_indirect(info);
	if (err) {
		xenbus_dev_fatal(info->xbdev, err, "setup_indirect at %s",
			info->xbdev->otherend);
		return;
	}

	err = xlvbd_alloc_gendisk(sectors, info, binfo, sector_size);
	if (err) {
		xenbus_dev_fatal(info->xbdev, err, "xlvbd_add at %s",
				 info->xbdev->otherend);
		return;
	}

	xenbus_switch_state(info->xbdev, XenbusStateConnected);

	/* Kick pending requests. */
	spin_lock_irq(&blkif_io_lock);
	info->connected = BLKIF_STATE_CONNECTED;
	kick_pending_request_queues(info);
	spin_unlock_irq(&blkif_io_lock);

	add_disk(info->gd);

	info->is_ready = 1;
}

/**
 * Callback received when the backend's state changes.
 */
static void blkback_changed(struct xenbus_device *dev,
				enum xenbus_state backend_state)
{
	struct blkfront_info *info = dev_get_drvdata(&dev->dev);

	printk("blkfront:blkback_changed to state %d.\n", backend_state);
	dev_dbg(&dev->dev, "blkfront:blkback_changed to state %d.\n", backend_state);

	switch (backend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitialised:
	case XenbusStateUnknown:
	case XenbusStateClosed:
		break;

	case XenbusStateInitWait:
		if (talk_to_blkback(dev, info)) {
			dev_set_drvdata(&dev->dev, NULL);
			kfree(info);
		}
		break;

	case XenbusStateConnected:
		blkfront_connect(info);
		break;

	case XenbusStateClosing:
		kthread_run(blkfront_closing, info, "xenwatch_unplugdisk");
		break;
	}
}

static int blkfront_remove(struct xenbus_device *xbdev)
{
	struct blkfront_info *info = dev_get_drvdata(&xbdev->dev);
	struct block_device *bdev = NULL;
	struct gendisk *disk;

	dev_dbg(&xbdev->dev, "%s removed", xbdev->nodename);

	blkif_free(info, 0);

	mutex_lock(&info->mutex);

	disk = info->gd;
	if (disk)
		bdev = bdget_disk(disk, 0);

	info->xbdev = NULL;
	mutex_unlock(&info->mutex);

	if (!bdev) {
		if (!info->users){
			kfree(info);
		}
		return 0;
	}

	/*
	 * The xbdev was removed before we reached the Closed
	 * state. See if it's safe to remove the disk. If the bdev
	 * isn't closed yet, we let release take care of it.
	 */

	mutex_lock(&bdev->bd_mutex);
	info = disk->private_data;

	if (info && !bdev->bd_openers) {
		xlvbd_release_gendisk(info);
		disk->private_data = NULL;
		kfree(info);
	}

	mutex_unlock(&bdev->bd_mutex);
	bdput(bdev);

	return 0;
}

static int blkfront_is_ready(struct xenbus_device *dev)
{
	struct blkfront_info *info = dev_get_drvdata(&dev->dev);

	return info->is_ready && info->xbdev;
}

static int blkif_open(struct block_device *bdev, fmode_t mode)
{
	struct gendisk *disk = bdev->bd_disk;
	struct blkfront_info *info;
	int err = 0;

	info = disk->private_data;
	if (!info) {
		/* xbdev gone */
		err = -ERESTARTSYS;
		goto out;
	}

	mutex_lock(&info->mutex);

	if (!info->gd)
		/* xbdev is closed */
		err = -ERESTARTSYS;
	else
		info->users++;

	mutex_unlock(&info->mutex);

out:
	return err;
}

static int blkif_release(struct gendisk *disk, fmode_t mode)
{
	struct blkfront_info *info = disk->private_data;
	struct block_device *bdev;
	struct xenbus_device *xbdev;

	bdev = bdget_disk(disk, 0);

	if ( info ){
		info->users--;
	} else {
		goto out;
	}

	/*
	 * Check if we have been instructed to close. We will have
	 * deferred this request, because the bdev was still open.
	 */

	mutex_lock(&info->mutex);
	xbdev = info->xbdev;

	if (xbdev && xbdev->state && xbdev->state == XenbusStateClosing) {
		/* pending switch to state closed */
		xlvbd_release_gendisk(info);
		xenbus_frontend_closed(info->xbdev);
 	}

	mutex_unlock(&info->mutex);

	if (!xbdev) {
		/* sudden device removal */
		xlvbd_release_gendisk(info);
		disk->private_data = NULL;
		if (!info->users){
			kfree(info);
		}
	}

out:
	bdput(bdev);
	return 0;
}

static const struct block_device_operations xlvbd_block_fops =
{
	.owner = THIS_MODULE,
	.open = blkif_open,
	.release = blkif_release,
	.getgeo = blkif_getgeo,
	.locked_ioctl = blkif_ioctl,
};


static struct xenbus_device_id blkfront_ids[] = {
	{ "vbd" },
	{ "" }
};

static struct xenbus_driver blkfront = {
	.name = "vbd",
	.owner = THIS_MODULE,
	.ids = blkfront_ids,
	.probe = blkfront_probe,
	.remove = blkfront_remove,
	.suspend = blkfront_suspend,
	.resume = blkfront_resume,
	.otherend_changed = blkback_changed,
	.is_ready = blkfront_is_ready,
};

static int __init xlblk_init(void)
{
	int ret;

	if (!xen_domain())
		return -ENODEV;

	if (xen_hvm_domain() && !xen_platform_pci_unplug)
		return -ENODEV;

	printk("%s: register_blkdev major: %d \n", __FUNCTION__, XENVBD_MAJOR);
	if (register_blkdev(XENVBD_MAJOR, DEV_NAME)) {
		printk(KERN_WARNING "xen_blk: can't get major %d with name %s\n",
			   XENVBD_MAJOR, DEV_NAME);
		return -ENODEV;
	}

	if (sda_is_xvda) {
		emulated_sd_disk_minor_offset = 0;
		emulated_sd_disk_name_offset = emulated_sd_disk_minor_offset / 256;
	}

	ret = xenbus_register_frontend(&blkfront);
	if (ret) {
		unregister_blkdev(XENVBD_MAJOR, DEV_NAME);
		return ret;
	}

	return 0;
}
module_init(xlblk_init);


static void __exit xlblk_exit(void)
{
	return xenbus_unregister_driver(&blkfront);
}
module_exit(xlblk_exit);

MODULE_DESCRIPTION("Xen virtual block device frontend");
MODULE_LICENSE("GPL");
MODULE_ALIAS_BLOCKDEV_MAJOR(XENVBD_MAJOR);
MODULE_ALIAS("xen:vbd");
MODULE_ALIAS("xenblk");
