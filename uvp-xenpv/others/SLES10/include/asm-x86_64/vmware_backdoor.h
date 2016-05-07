/*
 * Copyright (C) 2007, VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 and no later version.
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
 */

#ifndef _VMWARE_BACKDOOR_H
#define _VMWARE_BACKDOOR_H

#include <linux/types.h>

#define VMWARE_BDOOR_MAGIC     0x564D5868
#define VMWARE_BDOOR_PORT      0x5658

#define VMWARE_BDOOR_CMD_GETVERSION         10
#define VMWARE_BDOOR_CMD_GETHZ              45
#define VMWARE_BDOOR_CMD_LAZYTIMEREMULATION 49

#define VMWARE_BDOOR(cmd, eax, ebx, ecx, edx)                         \
        __asm__("inl (%%dx)" :                                        \
	        "=a"(eax), "=c"(ecx), "=d"(edx), "=b"(ebx) :          \
		"0"(VMWARE_BDOOR_MAGIC), "1"(VMWARE_BDOOR_CMD_##cmd), \
		"2"(VMWARE_BDOOR_PORT), "3"(0) :                      \
		"memory");


static inline int vmware_platform(void)
{
	uint32_t eax, ebx, ecx, edx;
	VMWARE_BDOOR(GETVERSION, eax, ebx, ecx, edx);
	return eax != (uint32_t)-1 && ebx == VMWARE_BDOOR_MAGIC;
}

static inline unsigned int vmware_get_tsc_khz(void)
{
	uint64_t tsc_hz;
	uint32_t eax, ebx, ecx, edx;

	VMWARE_BDOOR(GETHZ, eax, ebx, ecx, edx);

	if (eax == (uint32_t)-1) return 0;
	tsc_hz = eax | (((uint64_t)ebx) << 32);
	BUG_ON((tsc_hz / 1000) >> 32);
	return tsc_hz / 1000;
}

/* 
 * Usually, all timer devices in the VM are kept consistent, but this
 * can cause time in the VM to fall behind (e.g., if the VM is
 * descheduled for long periods of time, the hypervisor may not have had
 * an opportunity to deliver timer interrupts, and so the time in the VM
 * will be behind until it can catch up.  In lazy timer emulation mode,
 * the TSC tracks real time regardless of whether e.g. timer interrupts
 * are delivered on time.
 */
static inline int vmware_enable_lazy_timer_emulation(void)
{
	uint32_t eax, ebx, ecx, edx;
	VMWARE_BDOOR(LAZYTIMEREMULATION, eax, ebx, ecx, edx);
	return ebx == VMWARE_BDOOR_MAGIC;
}

#endif
