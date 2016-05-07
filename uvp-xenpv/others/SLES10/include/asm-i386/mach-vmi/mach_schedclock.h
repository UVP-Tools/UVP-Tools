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
 * Machine specific sched_clock routines.
 */

#ifndef __ASM_MACH_SCHEDCLOCK_H
#define __ASM_MACH_SCHEDCLOCK_H

#include <vmi_time.h>

#if !defined(CONFIG_X86_VMI)
# error invalid sub-arch include
#endif

static inline unsigned long long sched_clock_cycles(void)
{
	return vmi_get_available_cycles();
}

#endif /* __ASM_MACH_SCHEDCLOCK_H */
