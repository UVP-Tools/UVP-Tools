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

#ifndef __MACH_DESC_H
#define __MACH_DESC_H

#include <vmi.h>

#if !defined(CONFIG_X86_VMI)
# error invalid sub-arch include
#endif

static inline void load_gdt(struct Xgt_desc_struct *const dtr)
{
	vmi_wrap_call(
		SetGDT, "lgdt (%0)",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (dtr),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void load_idt(struct Xgt_desc_struct *const dtr)
{
	vmi_wrap_call(
		SetIDT, "lidt (%0)",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (dtr),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void load_ldt(const u16 sel)
{
	vmi_wrap_call(
		SetLDT, "lldt %w0",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (sel),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void load_tr(const u16 sel)
{
	vmi_wrap_call(
		SetTR, "ltr %w0",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (sel),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void store_gdt(struct Xgt_desc_struct *const dtr)
{
	vmi_wrap_call(
		GetGDT, "sgdt (%0)",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (dtr),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void store_idt(struct Xgt_desc_struct *const dtr)
{
	vmi_wrap_call(GetIDT, "sidt (%0)",
		VMI_NO_OUTPUT,
		1, VMI_IREG1 (dtr),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline unsigned vmi_get_ldt(void)
{
	unsigned ret;
	vmi_wrap_call(
		GetLDT, "sldt %%ax",
		VMI_OREG1 (ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

static inline unsigned vmi_get_tr(void)
{
	unsigned ret;
	vmi_wrap_call(
		GetTR, "str %%ax",
		VMI_OREG1 (ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

#define load_TR_desc() load_tr(GDT_ENTRY_TSS*8)
#define load_LDT_desc() load_ldt(GDT_ENTRY_LDT*8)
#define clear_LDT_desc() load_ldt(0)

#define store_tr(tr) do { (tr) = vmi_get_tr(); } while (0)
#define store_ldt(ldt) do { (ldt) = vmi_get_ldt(); } while (0)

static inline void vmi_write_gdt(void *gdt, unsigned entry, u32 descLo, u32 descHi)
{
	vmi_wrap_call(
			/*
			 * We want to write a two word descriptor to GDT entry
			 * number (%1) offset from *gdt (%0), so using 8 byte
			 * indexing, this can be expressed compactly as
			 *    (gdt,entry,8) and 4(gdt,entry,8)
			 */
		WriteGDTEntry, "movl %2, (%0,%1,8);"
			       "movl %3, 4(%0,%1,8);",
		VMI_NO_OUTPUT,
		4, VMI_XCONC(VMI_IREG1(gdt), VMI_IREG2(entry), VMI_IREG3(descLo), VMI_IREG4(descHi)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}


extern void vmi_write_ldt(void *ldt, unsigned entry, u32 descLo, u32 descHi);

static inline void vmi_write_idt(void *idt, unsigned entry, u32 descLo, u32 descHi)
{
	vmi_wrap_call(
		WriteIDTEntry, "movl %2, (%0,%1,8);"
			       "movl %3, 4(%0,%1,8);",
		VMI_NO_OUTPUT,
		4, VMI_XCONC(VMI_IREG1(idt), VMI_IREG2(entry), VMI_IREG3(descLo), VMI_IREG4(descHi)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void load_TLS(struct thread_struct *t, unsigned int cpu)
{
	unsigned i;
	struct desc_struct *gdt = get_cpu_gdt_table(cpu);
	for (i = 0; i < TLS_SIZE / sizeof(struct desc_struct); i++) {
		unsigned cur = i + GDT_ENTRY_TLS_MIN;
		if (gdt[cur].a != t->tls_array[i].a || gdt[cur].b != t->tls_array[i].b) {
			vmi_write_gdt(gdt, cur, t->tls_array[i].a, t->tls_array[i].b);
		}
        }
}

static inline void write_gdt_entry(void *gdt, int entry, u32 entry_a, u32 entry_b)
{
        vmi_write_gdt(gdt, entry, entry_a, entry_b);
}

static inline void write_ldt_entry(void *ldt, int entry, u32 entry_a, u32 entry_b)
{
	vmi_write_ldt(ldt, entry, entry_a, entry_b);
}

static inline void write_idt_entry(void *idt, int entry, u32 entry_a, u32 entry_b)
{
        vmi_write_idt(idt, entry, entry_a, entry_b);
}
#endif
