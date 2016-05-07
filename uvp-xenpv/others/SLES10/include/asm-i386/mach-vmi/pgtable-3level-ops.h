#ifndef _MACH_PGTABLE_LEVEL_OPS_H
#define _MACH_PGTABLE_LEVEL_OPS_H

/*
 * CPUs that support out of order store must provide a write memory barrier
 * to ensure that the TLB does not prefetch an entry with only the low word
 * written.  We are guaranteed that the TLB can not prefetch a partially
 * written entry when writing the high word first, since by calling
 * convention, set_pte can only be called when the entry is either not
 * present, or not accessible by any TLB.
 */
#ifdef CONFIG_X86_OOSTORE
#define PAE_PTE_WMB "lock; addl $0,0(%%esp);"
#else
#define PAE_PTE_WMB
#endif

static inline void vmi_set_pxe(pte_t pteval, pte_t *ptep, u32 flags)
{
	vmi_wrap_call(
		SetPxELong, "movl %1, 4(%2);"
			    PAE_PTE_WMB
			    "movl %0, (%2);",
		VMI_NO_OUTPUT,
		4, VMI_XCONC(VMI_IREG1(pteval.pte_low), VMI_IREG2(pteval.pte_high),
			 VMI_IREG3(ptep), VMI_IREG4(flags)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

/*
 * Since this is only called on user PTEs, and the page fault handler
 * must handle the already racy situation of simultaneous page faults,
 * we are justified in merely clearing the PTE, followed by a set.
 * The ordering here is important.
 */
static inline void vmi_set_pxe_present(pte_t pte, pte_t *ptep, u32 flags)
{
	vmi_wrap_call(
		SetPxELong, "movl $0, (%2);"
			    PAE_PTE_WMB
			    "movl %1, 4(%2);"
			    PAE_PTE_WMB
			    "movl %0, (%2);",
		VMI_NO_OUTPUT,
		4, VMI_XCONC(VMI_IREG1(pte.pte_low), VMI_IREG2(pte.pte_high),
			 VMI_IREG3(ptep), VMI_IREG4(flags)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory", "ebx", "edi",
						   "eax", "edx"));
}

static inline void vmi_clear_pxe(pte_t *ptep, u32 flags)
{
	vmi_wrap_call(
		SetPxELong, "movl %0, (%2);"
			    PAE_PTE_WMB
			    "movl %1, 4(%2);",
		VMI_NO_OUTPUT,
		4, VMI_XCONC(VMI_IREG1(0), VMI_IREG2(0),
			 VMI_IREG3(ptep), VMI_IREG4(flags)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void vmi_notify_pte_update(pte_t *ptep, u32 flags)
{
	vmi_wrap_call(
		UpdatePxELong, "",
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(ptep), VMI_IREG2(flags)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void vmi_set_pxe_atomic(pte_t *ptep, unsigned long long pteval)
{
	set_64bit((unsigned long long *)ptep, pteval);
	vmi_notify_pte_update(ptep, VMI_PAGE_PT);
}

/* Called only for kernel PTEs and PMDs (set_pmd_pte) */
#define set_pte_atomic(ptep, pte)					\
	 vmi_set_pxe_atomic(ptep, pte_val(pte))

static inline void set_pmd(pmd_t *pmdptr, pmd_t pmdval)
{
	vmi_set_pxe_atomic((pte_t *)pmdptr, pmd_val(pmdval));
}

#define set_pud(pudptr,pudval) \
	        (*(pudptr) = (pudval))

#endif
