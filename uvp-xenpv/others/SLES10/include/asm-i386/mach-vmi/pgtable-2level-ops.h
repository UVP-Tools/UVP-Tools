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


#ifndef _MACH_PGTABLE_LEVEL_OPS_H
#define _MACH_PGTABLE_LEVEL_OPS_H

static inline void vmi_set_pxe(pte_t pteval, pte_t *ptep, u32 flags)
{
	vmi_wrap_call(
		SetPxE, "movl %0, (%1)",
		VMI_NO_OUTPUT,
		3, VMI_XCONC(VMI_IREG1(pteval), VMI_IREG2(ptep), VMI_IREG3(flags)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

#define vmi_set_pxe_present(pteval, ptep, flags) vmi_set_pxe((pteval), (ptep), (flags))
#define vmi_clear_pxe(ptep, flags) vmi_set_pxe((pte_t) {0}, (ptep), (flags))

/*
 * (pmds are folded into pgds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pmd(pmdptr, pmdval) \
	vmi_set_pxe((pte_t) {(pmdval).pud.pgd.pgd}, (pte_t *) pmdptr, VMI_PAGE_PD)

static inline void vmi_notify_pte_update(pte_t *ptep, u32 flags)
{
	vmi_wrap_call(
		UpdatePxE, "",
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(ptep), VMI_IREG2(flags)),
		VMI_CLOBBER(ZERO_RETURNS));
}

#endif /* _MACH_PGTABLE_LEVEL_OPS_H */
