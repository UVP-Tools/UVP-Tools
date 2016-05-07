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


#ifndef _MACH_TLBFLUSH_H
#define _MACH_TLBFLUSH_H

#include <vmi.h>

static inline void __flush_tlb(void)
{
	vmi_wrap_call(
		FlushTLB, "mov %%cr3, %%eax; mov %%eax, %%cr3",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(VMI_FLUSH_TLB),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "eax", "memory"));
}

static inline void __flush_tlb_global(void)
{
	vmi_wrap_call(
		FlushTLB, "mov %%cr4, %%eax;	\n"
			  "andb $0x7f, %%al;	\n"
			  "mov %%eax, %%cr4;	\n"
			  "orb $0x80, %%al;	\n"
			  "mov %%eax, %%cr4",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(VMI_FLUSH_TLB | VMI_FLUSH_GLOBAL),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "eax", "memory"));
}

static inline void __flush_tlb_single(u32 va)
{
	vmi_wrap_call(
		InvalPage, "invlpg (%0)",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(va),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

#endif /* _MACH_TLBFLUSH_H */
