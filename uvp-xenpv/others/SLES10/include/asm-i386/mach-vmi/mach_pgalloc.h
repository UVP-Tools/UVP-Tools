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

#ifndef _MACH_PGALLOC_H
#define _MACH_PGALLOC_H

#include <vmi.h>

static inline void vmi_allocate_page(u32 pfn, u32 flags, u32 clone_pfn, u32 base, u32 count)
{
	vmi_wrap_call(
		AllocatePage, "",
		VMI_NO_OUTPUT,
		5, VMI_XCONC(VMI_IREG1(pfn), VMI_IREG2(flags), VMI_IREG3(clone_pfn),
		         VMI_IREG4(base), VMI_IREG5(count)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void vmi_release_page(u32 pfn, u32 flags)
{
	vmi_wrap_call(
		ReleasePage, "",
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(pfn), VMI_IREG2(flags)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

#ifndef CONFIG_X86_PAE

#define mach_setup_pte(va, pfn)                            \
do {                                                       \
	vmi_allocate_page(pfn, VMI_PAGE_ZEROED |	   \
			       VMI_PAGE_PT, 0, 0, 0);	   \
} while (0)

#define mach_release_pte(va, pfn)                          \
do {                                                       \
       	vmi_release_page(pfn, VMI_PAGE_PT);		   \
} while (0)

#define mach_setup_pmd(va, pfn)
#define mach_release_pmd(va, pfn)

#define vmi_setup_pgd(va, pfn, root, base, count)                  \
do {                                                               \
        vmi_allocate_page(pfn, VMI_PAGE_ZEROED | VMI_PAGE_CLONE |  \
			       VMI_PAGE_PD, root, base, count);    \
} while (0)

#define vmi_release_pgd(va, pfn)                                   \
do {                                                               \
	vmi_release_page(pfn, VMI_PAGE_PD);                	   \
} while (0)

#else /* CONFIG_X86_PAE */

#define mach_setup_pte(va, pfn)                                 \
do {                                                            \
	vmi_allocate_page(pfn, VMI_PAGE_ZEROED | VMI_PAGE_PAE |	\
			       VMI_PAGE_PT, 0, 0, 0);	   	\
} while (0)

#define mach_release_pte(va, pfn)                          \
do {                                                       \
       	vmi_release_page(pfn, VMI_PAGE_PAE | VMI_PAGE_PT); \
} while (0)

#define mach_setup_pmd(va, pfn)                                 \
do {                                                            \
       vmi_allocate_page(pfn, VMI_PAGE_ZEROED | VMI_PAGE_PAE |	\
			       VMI_PAGE_PD, 0, 0, 0);	   	\
} while (0)

#define mach_release_pmd(va, pfn)                          \
do {                                                       \
       	vmi_release_page(pfn, VMI_PAGE_PAE|VMI_PAGE_PD);   \
} while (0)

#define vmi_setup_pgd(va, pfn, root, base, count)          \
        vmi_allocate_page(pfn, VMI_PAGE_PDP, 0, 0, 0)

#define vmi_release_pgd(va, pfn)                           \
	vmi_release_page(pfn, VMI_PAGE_PDP)

#endif

#define mach_setup_pgd(va, pfn, root, base, count)	\
do {							\
	vmi_setup_pgd(va, pfn, root, base, count);	\
} while (0)

#define mach_release_pgd(va, pfn)                       \
do {                                                    \
	vmi_release_pgd(va, pfn);			\
} while (0)

static inline void vmi_set_linear_mapping(const int slot, const u32 va, const u32 pages, const u32 pfn)
{
	vmi_wrap_call(
		SetLinearMapping, "",
		VMI_NO_OUTPUT,
		4, VMI_XCONC(VMI_IREG1(slot), VMI_IREG2(va), VMI_IREG3(pages), VMI_IREG4(pfn)),
		VMI_CLOBBER(ZERO_RETURNS));
}

#define mach_map_linear_pt(num, ptep, pfn) \
	vmi_set_linear_mapping(num+1, (u32)ptep, 1, pfn)
#define mach_map_linear_ldt(ldt, pfn) \
	vmi_set_linear_mapping(3, (u32)ldt, 1, pfn)
#define mach_map_linear_range(start, pages, pfn) \
	vmi_set_linear_mapping(0, start, pages, pfn)

#endif /* _MACH_PGALLOC_H */
