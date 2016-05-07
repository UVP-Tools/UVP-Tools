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
 * Send feedback to dhecht@vmware.com
 *
 */

/*
 * Initiation routines related to the APIC timer.
 * These may optionally be overridden by the subarchitecture in
 * mach_apictimer.h.
 */

#ifndef __ASM_MACH_APICTIMER_H
#define __ASM_MACH_APICTIMER_H

#ifdef CONFIG_X86_LOCAL_APIC

extern int __init timer_irq_works(void);

static inline void mach_setup_boot_local_clock(void)
{
	setup_boot_APIC_clock();
}

static inline void mach_setup_secondary_local_clock(void)
{
	setup_secondary_APIC_clock();
}

static inline int mach_timer_irq_works(void)
{
	return timer_irq_works();
}

#endif /* CONFIG_X86_LOCAL_APIC */

#endif /* __ASM_MACH_APICTIMER_H */
