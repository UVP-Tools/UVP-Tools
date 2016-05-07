/*
 * Copyright (C) 2005, VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to zach@vmware.com
 *
 */


#ifndef _MACH_PGTABLE_H
#define _MACH_PGTABLE_H

#include <vmi.h>

#define __HAVE_SUBARCH_PTE_WRITE_FUNCTIONS

#define is_current_as(mm, mustbeuser) ((mm) == current->active_mm ||	\
				       (!mustbeuser && (mm) == &init_mm))

#define vmi_flags_addr(mm, addr, level, user)				\
	((level) | (is_current_as(mm, user) ? 				\
		(VMI_PAGE_CURRENT_AS | ((addr) & VMI_PAGE_VA_MASK)) : 0))

#define vmi_flags_addr_defer(mm, addr, level, user)			\
	((level) | (is_current_as(mm, user) ? 				\
		(VMI_PAGE_DEFER | VMI_PAGE_CURRENT_AS | ((addr) & VMI_PAGE_VA_MASK)) : 0))

/*
 * Rules for using set_pte: the pte being assigned *must* be
 * either not present or in a state where the hardware will
 * not attempt to update the pte.  In places where this is
 * not possible, use pte_get_and_clear to obtain the old pte
 * value and then use set_pte to update it.  -ben
 */
#define set_pte(ptep, pte)						\
	vmi_set_pxe(pte, ptep, VMI_PAGE_PT)

/*
 * Rules for using set_pte_at: the pte may be a kernel or user
 * page table entry, and must be not present or not mapped. Use
 * this in places where the mm and address information has been
 * preserved.
 */
#define set_pte_at(mm, addr, ptep, pte)					\
	vmi_set_pxe(pte, ptep, vmi_flags_addr(mm, addr, VMI_PAGE_PT, 0))

/*
 * Rules for using set_pte_at_defer: the pte may be a kernel or
 * user page table entry, and must be not present or a private
 * map. Use this in places where the mm and address information
 * has been preserved and the page update is followed by an
 * immediate flush.
 */
#define set_pte_at_defer(mm, addr, ptep, pte)				\
	vmi_set_pxe(pte, ptep, vmi_flags_addr_defer(mm, addr, VMI_PAGE_PT, 0))

#ifdef CONFIG_X86_PAE
# include <pgtable-3level-ops.h>
#else
# include <pgtable-2level-ops.h>
#endif

/*
 * Rules for using ptep_establish: the pte MUST be a user pte, and must be a
 * present->present transition.
 */
#define __HAVE_ARCH_PTEP_ESTABLISH
#define ptep_establish(__vma, __address, __ptep, __entry)		\
do {				  					\
	vmi_set_pxe_present(__entry, __ptep,				\
			    vmi_flags_addr_defer((__vma)->vm_mm,	\
					(__address), VMI_PAGE_PT, 1));	\
	flush_tlb_page(__vma, __address);				\
} while (0)

/*
 * Rules for using ptep_set_access_flags: the pte MUST be a user PTE, and
 * must only modify accessed / dirty bits.
 */
#define __HAVE_ARCH_PTEP_SET_ACCESS_FLAGS
#define ptep_set_access_flags(__vma, __address, __ptep, __entry, __dirty) \
do {									  \
	if (__dirty) {							  \
		vmi_set_pxe(__entry, __ptep, 				  \
			    vmi_flags_addr_defer((__vma)->vm_mm,	  \
					(__address), VMI_PAGE_PT, 1));	  \
		flush_tlb_page(__vma, __address);			  \
	}								  \
} while (0)

#define pte_clear(__mm, __address, __ptep)				\
	vmi_clear_pxe((__ptep), vmi_flags_addr((__mm), (__address),	\
					       VMI_PAGE_PT, 0))

#define pmd_clear(__pmdptr)						\
	vmi_clear_pxe((pte_t *)(__pmdptr), VMI_PAGE_PD)


#define __HAVE_ARCH_PTE_CLEAR_NOT_PRESENT_FULL
#define pte_clear_not_present_full(__mm, __address, __ptep, __full)	\
do {					       				\
	_pte_clear(__ptep);					\
} while (0)

static inline void vmi_set_lazy_mode(u32 lazy_mode)
{
	vmi_wrap_call(
		SetLazyMode, "",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(lazy_mode),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

#define __HAVE_ARCH_ENTER_LAZY_CPU_MODE
static inline void arch_enter_lazy_cpu_mode(void)
{
	vmi_set_lazy_mode(VMI_LAZY_CPU_STATE);
}

static inline void arch_leave_lazy_cpu_mode(void)
{
	vmi_set_lazy_mode(VMI_LAZY_MODE_OFF);
}

#define __HAVE_ARCH_ENTER_LAZY_MMU_MODE
static inline void arch_enter_lazy_mmu_mode(void)
{
	vmi_set_lazy_mode(VMI_LAZY_MMU_UPDATES);
}

static inline void arch_leave_lazy_mmu_mode(void)
{
	vmi_set_lazy_mode(VMI_LAZY_MODE_OFF);
}

/*
 * Rulese for using pte_update_hook - it must be called after any PTE
 * update which has not been done using the set_pte interfaces.  Kernel
 * PTE updates should either be sets, clears, or set_pte_atomic for P->P
 * transitions, which means this hook should only be called for user PTEs.
 * This hook implies a P->P protection or access change has taken place,
 * which requires a subsequent TLB flush.  This is why we allow the
 * notification to be deferred.
 */
#define pte_update_hook(mm, addr, ptep)					\
	vmi_notify_pte_update(ptep, vmi_flags_addr_defer(mm, addr, VMI_PAGE_PT, 1));

#endif /* _PGTABLE_OPS_H */
