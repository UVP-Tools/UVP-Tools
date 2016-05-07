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


#ifndef _MACH_PROCESSOR_H
#define _MACH_PROCESSOR_H

#include <vmi.h>

static inline void vmi_cpuid(const int op, int *eax, int *ebx, int *ecx, int *edx)
{
	vmi_wrap_call(
		CPUID, "cpuid",
		VMI_XCONC("=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)),
		1, "a" (op),
		VMI_CLOBBER(FOUR_RETURNS));
}

/*
 * Generic CPUID function
 */
static inline void cpuid(int op, int *eax, int *ebx, int *ecx, int *edx)
{
	vmi_cpuid(op, eax, ebx, ecx, edx);
}


/* Some CPUID calls want 'count' to be placed in ecx */
static inline void cpuid_count(int op, int count, int *eax, int *ebx, int *ecx,
	       	int *edx)
{
	vmi_wrap_call(
		CPUID, "cpuid",
		VMI_XCONC("=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)),
		2, VMI_XCONC("a" (op), "c" (count)),
		VMI_CLOBBER(FOUR_RETURNS));
}

/*
 * CPUID functions returning a single datum
 */
static inline unsigned int cpuid_eax(unsigned int op)
{
	unsigned int eax, ebx, ecx, edx;

	vmi_cpuid(op, &eax, &ebx, &ecx, &edx);
	return eax;
}

static inline unsigned int cpuid_ebx(unsigned int op)
{
	unsigned int eax, ebx, ecx, edx;

	vmi_cpuid(op, &eax, &ebx, &ecx, &edx);
	return ebx;
}

static inline unsigned int cpuid_ecx(unsigned int op)
{
	unsigned int eax, ebx, ecx, edx;

	vmi_cpuid(op, &eax, &ebx, &ecx, &edx);
	return ecx;
}

static inline unsigned int cpuid_edx(unsigned int op)
{
	unsigned int eax, ebx, ecx, edx;

	vmi_cpuid(op, &eax, &ebx, &ecx, &edx);
	return edx;
}

static inline void arch_update_kernel_stack(unsigned sel, u32 stack)
{
	vmi_wrap_call(
		UpdateKernelStack, "",
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(sel), VMI_IREG2(stack)),
		VMI_CLOBBER(ZERO_RETURNS));
}

static inline void set_debugreg(const u32 val, const int num)
{
	vmi_wrap_call(
		SetDR, "movl %1, %%db%c2",
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(num), VMI_IREG2(val), VMI_IMM (num)),
		VMI_CLOBBER(ZERO_RETURNS));
}

static inline u32 vmi_get_dr(const int num)
{
	u32 ret;
	vmi_wrap_call(
		GetDR, "movl %%db%c1, %%eax",
		VMI_OREG1(ret),
		1, VMI_XCONC(VMI_IREG1(num), VMI_IMM (num)),
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

#define get_debugreg(var, register) do { var = vmi_get_dr(register); } while (0)

static inline void set_iopl_mask(u32 mask)
{
	vmi_wrap_call(
		SetIOPLMask,	"pushfl;"
				"andl $0xffffcfff, (%%esp);"
				"orl %0, (%%esp);"
				"popfl",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (mask),
		VMI_CLOBBER(ZERO_RETURNS));
}

#endif
