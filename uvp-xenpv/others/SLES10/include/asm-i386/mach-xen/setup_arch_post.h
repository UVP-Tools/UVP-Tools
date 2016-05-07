/**
 * machine_specific_memory_setup - Hook for machine specific memory setup.
 *
 * Description:
 *	This is included late in kernel/setup.c so that it can make
 *	use of all of the static functions.
 **/

static char * __init machine_specific_memory_setup(void)
{
	int rc;
	struct xen_memory_map memmap;
	/*
	 * This is rather large for a stack variable but this early in
	 * the boot process we know we have plenty slack space.
	 */
	struct e820entry map[E820MAX];

	memmap.nr_entries = E820MAX;
	set_xen_guest_handle(memmap.buffer, map);

	rc = HYPERVISOR_memory_op(XENMEM_memory_map, &memmap);
	if ( rc == -ENOSYS ) {
		memmap.nr_entries = 1;
		map[0].addr = 0ULL;
		map[0].size = PFN_PHYS((unsigned long long)xen_start_info->nr_pages);
		/* 8MB slack (to balance backend allocations). */
		map[0].size += 8ULL << 20;
		map[0].type = E820_RAM;
		rc = 0;
	}
	BUG_ON(rc);

	sanitize_e820_map(map, (char *)&memmap.nr_entries);

	BUG_ON(copy_e820_map(map, (char)memmap.nr_entries) < 0);

	return "Xen";
}
