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


#ifndef _MACH_SYSTEM_H
#define _MACH_SYSTEM_H

#include <vmi.h>

static inline void write_cr0(const u32 val)
{
	vmi_wrap_call(
		SetCR0, "mov %0, %%cr0",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(val),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void write_cr2(const u32 val)
{
	vmi_wrap_call(
		SetCR2, "mov %0, %%cr2",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(val),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void write_cr3(const u32 val)
{
	vmi_wrap_call(
		SetCR3, "mov %0, %%cr3",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(val),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void write_cr4(const u32 val)
{
	vmi_wrap_call(
		SetCR4, "mov %0, %%cr4",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(val),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline u32 read_cr0(void)
{
	u32 ret;
	vmi_wrap_call(
		GetCR0, "mov %%cr0, %%eax",
		VMI_OREG1(ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

static inline u32 read_cr2(void)
{
	u32 ret;
	vmi_wrap_call(
		GetCR2, "mov %%cr2, %%eax",
		VMI_OREG1(ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

static inline u32 read_cr3(void)
{
	u32 ret;
	vmi_wrap_call(
		GetCR3, "mov %%cr3, %%eax",
		VMI_OREG1(ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

static inline u32 read_cr4(void)
{
	u32 ret;
	vmi_wrap_call(
		GetCR4, "mov %%cr4, %%eax",
		VMI_OREG1(ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

#define read_cr4_safe() read_cr4()
#define load_cr3(pgdir) write_cr3(__pa(pgdir))

static inline void clts(void)
{
	vmi_wrap_call(
		CLTS, "clts",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ZERO_RETURNS));
}

static inline void wbinvd(void)
{
	vmi_wrap_call(
		WBINVD, "wbinvd",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

/*
 * For EnableInterrupts, DisableInterrupts, GetInterruptMask, SetInterruptMask,
 * only flags are clobbered by these calls, since they have assembler call
 * convention.  We can get better C code by indicating only "cc" clobber.
 * Both setting and disabling interrupts must use memory clobber as well, to
 * prevent GCC from reordering memory access around them.
 */
static inline void local_irq_disable(void)
{
	vmi_wrap_call(
		DisableInterrupts, "cli",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_XCONC("cc", "memory"));
}

static inline void local_irq_enable(void)
{
	vmi_wrap_call(
		EnableInterrupts, "sti",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_XCONC("cc", "memory"));
}

static inline void local_irq_restore(const unsigned long flags)
{
	vmi_wrap_call(
		SetInterruptMask, "pushl %0; popfl",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (flags),
		VMI_XCONC("cc", "memory"));
}

static inline unsigned long vmi_get_flags(void)
{
	unsigned long ret;
	vmi_wrap_call(
		GetInterruptMask, "pushfl; popl %%eax",
		VMI_OREG1 (ret),
		0, VMI_NO_INPUT,
		"cc");
	return ret;
}

#define local_save_flags(x)     do { typecheck(unsigned long,x); (x) = vmi_get_flags(); } while (0)

static inline void vmi_reboot(int how)
{
	vmi_wrap_call(
		Reboot, "",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(how),
		"memory"); /* only memory clobber for better code */
}

static inline void safe_halt(void)
{
	vmi_wrap_call(
		Halt, "sti; hlt",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ZERO_RETURNS));
}

/* By default, halt is assumed safe, but we can drop the sti */
static inline void halt(void)
{
	vmi_wrap_call(
		Halt, "hlt",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ZERO_RETURNS));
}

static inline void shutdown_halt(void)
{
	vmi_wrap_call(
		Shutdown, "cli; hlt",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		"memory"); /* only memory clobber for better code */
}

#endif
