#ifndef _MACH_PGTABLE_H
#define _MACH_PGTABLE_H

/* Update hook for shadow mode hypervisors */
#define pte_update_hook(mm, addr, pteptr) do { } while (0)

/*
 * Also, we only update the dirty/accessed state if we set
 * the dirty bit by hand in the kernel, since the hardware
 * will do the accessed bit for us, and we don't want to
 * race with other CPU's that might be updating the dirty
 * bit at the same time.
 */
#define __HAVE_ARCH_PTEP_SET_ACCESS_FLAGS
#define ptep_set_access_flags(__vma, __address, __ptep, __entry, __dirty) \
	do {								  \
		if (__dirty) {						  \
			(__ptep)->pte_low = (__entry).pte_low;	  	  \
			flush_tlb_page(__vma, __address);		  \
		}							  \
	} while (0)

#endif /* _PGTABLE_OPS_H */
