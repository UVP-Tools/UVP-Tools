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
 * Send feedback to arai@vmware.com
 *
 */

/*
 * VMI implementation for basic functions accessing APICs.  Calls into VMI
 * to perform operations.
 */

#ifndef __ASM_MACH_APICOPS_H

#if defined(CONFIG_VMI_APIC_HYPERCALLS) && defined(CONFIG_X86_LOCAL_APIC)
#define __ASM_MACH_APICOPS_H
#include <vmi.h>

static inline void apic_write(unsigned long reg, unsigned long value)
{
        void *addr = (void *)(APIC_BASE + reg);
	vmi_wrap_call(
		APICWrite, "movl %1, (%0)",
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(addr), VMI_IREG2(value)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

/*
 * Workaround APIC errata with serialization by using locked instruction;
 * See Pentium erratum 11AP
 */
static inline void apic_write_atomic(unsigned long reg, unsigned long value)
{
        void *addr = (void *)(APIC_BASE + reg);
	vmi_wrap_call(
		APICWrite, "xchgl %1, (%0)",
		/* Actually, there is unused output in %edx, clobbered below */
		VMI_NO_OUTPUT,
		2, VMI_XCONC(VMI_IREG1(addr), VMI_IREG2(value)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline unsigned long apic_read(unsigned long reg)
{
	unsigned long value;
        void *addr = (void *)(APIC_BASE + reg);
	vmi_wrap_call(
		APICRead, "movl (%0), %%eax",
		VMI_OREG1(value),
		1,VMI_IREG1(addr),
		VMI_CLOBBER(ONE_RETURN));
	return value;
}

#else
#include <../mach-default/mach_apicops.h>
#endif

#endif /* __ASM_MACH_APICOPS_H */
