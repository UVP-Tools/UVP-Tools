/*
 * paravirtualInterface.h --
 *
 *      Header file for paravirtualization interface and definitions
 *      for the hypervisor option ROM tables.
 *
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

#ifndef _PARAVIRTUAL_INTERFACE_H_
#define _PARAVIRTUAL_INTERFACE_H_

#include "vmiCalls.h"

/*
 *---------------------------------------------------------------------
 *
 *  VMI Option ROM API
 *
 *---------------------------------------------------------------------
 */
#define VMI_SIGNATURE 0x696d5663   /* "cVmi" */

#define VMI_API_REV_MAJOR	3
/* The minor number we require for compatibility */
#define VMI_API_REV_MINOR	0

/* VMI Relocation types */
#define VMI_RELOCATION_NONE	0
#define VMI_RELOCATION_CALL_REL	1
#define VMI_RELOCATION_JUMP_REL	2

#ifndef __ASSEMBLY__
struct vmi_relocation_info {
	unsigned long		eip;
	unsigned char		type;
	unsigned char		reserved[3];
};
#endif

/* Flags used by VMI_Reboot call */
#define VMI_REBOOT_SOFT          0x0
#define VMI_REBOOT_HARD          0x1

/* Flags used by VMI_{Allocate|Release}Page call */
#define VMI_PAGE_PAE             0x10 /* Allocate PAE shadow */
#define VMI_PAGE_CLONE           0x20 /* Clone from another shadow */
#define VMI_PAGE_ZEROED          0x40 /* Page is pre-zeroed */

/* Flags shared by Allocate|Release Page and PTE updates */
#define VMI_PAGE_PT              0x01
#define VMI_PAGE_PD              0x02
#define VMI_PAGE_PDP             0x04
#define VMI_PAGE_PML4            0x08

/* Flags used by PTE updates */
#define VMI_PAGE_CURRENT_AS      0x10 /* implies VMI_PAGE_VA_MASK is valid */
#define VMI_PAGE_DEFER           0x20 /* may queue update until TLB inval */
#define VMI_PAGE_VA_MASK         0xfffff000

/* Flags used by VMI_FlushTLB call */
#define VMI_FLUSH_TLB            0x01
#define VMI_FLUSH_GLOBAL         0x02

/* Flags used by VMI_SetLazyMode */
#define VMI_LAZY_MODE_OFF	 0x00
#define VMI_LAZY_MMU_UPDATES     0x01
#define VMI_LAZY_CPU_STATE       0x02

/* The number of VMI address translation slot */
#define VMI_LINEAR_MAP_SLOTS    4

/* The cycle counters. */
#define VMI_CYCLES_REAL         0
#define VMI_CYCLES_AVAILABLE    1
#define VMI_CYCLES_STOLEN       2

/* The alarm interface 'flags' bits. [TBD: exact format of 'flags'] */
#define VMI_ALARM_COUNTERS      2

#define VMI_ALARM_COUNTER_MASK  0x000000ff

#define VMI_ALARM_WIRED_IRQ0    0x00000000
#define VMI_ALARM_WIRED_LVTT    0x00010000

#define VMI_ALARM_IS_ONESHOT    0x00000000
#define VMI_ALARM_IS_PERIODIC   0x00000100


/*
 *---------------------------------------------------------------------
 *
 *  Generic VROM structures and definitions
 *
 *---------------------------------------------------------------------
 */

#ifndef __ASSEMBLY__

struct vrom_header {
	u16	rom_signature;	// option ROM signature
	u8	rom_length;	// ROM length in 512 byte chunks
	u8	rom_entry[4];	// 16-bit code entry point
	u8	rom_pad0;	// 4-byte align pad
	u32	vrom_signature;	// VROM identification signature
	u8	api_version_min;// Minor version of API
	u8	api_version_maj;// Major version of API
	u8	jump_slots;	// Number of jump slots
	u8	reserved1;	// Reserved for expansion
	u32	reserved2;	// Reserved for expansion
	u32	reserved3;	// Reserved for private use
	u16	pci_header_offs;// Offset to PCI OPROM header
	u16	pnp_header_offs;// Offset to PnP OPROM header
	u32	rom_pad3;	// PnP reserverd / VMI reserved
	u8	reserved[96];	// Reserved for headers
	char	vmi_init[8];	// VMI_Init jump point
	char	get_reloc[8];	// VMI_GetRelocationInfo jump point
};

/* State needed to start an application processor in an SMP system. */
struct vmi_ap_state {
	u32 cr0;
	u32 cr2;
	u32 cr3;
	u32 cr4;

	u64 efer;

	u32 eip;
	u32 eflags;
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
	u32 esp;
	u32 ebp;
	u32 esi;
	u32 edi;
	u16 cs;
	u16 ss;
	u16 ds;
	u16 es;
	u16 fs;
	u16 gs;
	u16 ldtr;

	u16 gdtr_limit;
	u32 gdtr_base;
	u32 idtr_base;
	u16 idtr_limit;
};

#endif

#endif
