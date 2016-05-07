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
 * NO_IDLE_HZ callbacks.
 */

#ifndef __ASM_MACH_IDLETIMER_H
#define __ASM_MACH_IDLETIMER_H

#include <vmi.h>
#include <vmi_time.h>

#if !defined(CONFIG_X86_VMI)
# error invalid sub-arch include
#endif

#ifdef CONFIG_NO_IDLE_HZ

extern DEFINE_PER_CPU(unsigned char, one_shot_mode);

extern void vmi_stop_hz_timer(void);
extern void vmi_account_time_restart_hz_timer(struct pt_regs *regs, int cpu);

static inline void stop_hz_timer(void)
{
	if (vmi_timer_used())
		vmi_stop_hz_timer();
}

static inline void restart_hz_timer(struct pt_regs *regs)
{
	int cpu = smp_processor_id();
	/* nohz_cpu_mask bit set also implies vmi_timer_used(). */
	if (per_cpu(one_shot_mode, cpu))
		vmi_account_time_restart_hz_timer(regs, cpu);
}

#else /* CONFIG_NO_IDLE_HZ */

static inline void stop_hz_timer(void)
{
}

static inline void restart_hz_timer(struct pt_regs *regs)
{
}

#endif /* CONFIG_NO_IDLE_HZ */

#endif /* __ASM_MACH_IDLETIMER_H */
