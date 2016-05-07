/*
 * VMI Time wrappers
 *
 * Copyright (C) 2006, VMware, Inc.
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

#ifndef __MACH_VMI_TIME_H
#define __MACH_VMI_TIME_H

#include "vmi.h"

extern int hypervisor_timer_found;
extern void probe_vmi_timer(void);

static inline int vmi_timer_used(void)
{
	return hypervisor_timer_found;
}

/*
 * When run under a hypervisor, a vcpu is always in one of three states:
 * running, halted, or ready.  The vcpu is in the 'running' state if it
 * is executing.  When the vcpu executes the halt interface, the vcpu
 * enters the 'halted' state and remains halted until there is some work
 * pending for the vcpu (e.g. an alarm expires, host I/O completes on
 * behalf of virtual I/O).  At this point, the vcpu enters the 'ready'
 * state (waiting for the hypervisor to reschedule it).  Finally, at any
 * time when the vcpu is not in the 'running' state nor the 'halted'
 * state, it is in the 'ready' state.
 *
 * Real time is advances while the vcpu is 'running', 'ready', or
 * 'halted'.  Stolen time is the time in which the vcpu is in the
 * 'ready' state.  Available time is the remaining time -- the vcpu is
 * either 'running' or 'halted'.
 *
 * All three views of time are accessible through the VMI cycle
 * counters.
 */

static inline u64 vmi_get_cycle_frequency(void)
{
	u64 ret;
	vmi_wrap_call(
		GetCycleFrequency, "xor %%eax, %%eax;"
				   "xor %%edx, %%edx;",
		VMI_OREG64 (ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(TWO_RETURNS));
	return ret;
}

static inline u64 vmi_get_real_cycles(void)
{
	u64 ret;
	vmi_wrap_call(
		GetCycleCounter, "rdtsc",
		VMI_OREG64 (ret),
		1, VMI_IREG1(VMI_CYCLES_REAL),
		VMI_CLOBBER(TWO_RETURNS));
	return ret;
}

static inline u64 vmi_get_available_cycles(void)
{
	u64 ret;
	vmi_wrap_call(
		GetCycleCounter, "rdtsc",
		VMI_OREG64 (ret),
		1, VMI_IREG1(VMI_CYCLES_AVAILABLE),
		VMI_CLOBBER(TWO_RETURNS));
	return ret;
}

static inline u64 vmi_get_stolen_cycles(void)
{
	u64 ret;
	vmi_wrap_call(
		GetCycleCounter, "xor %%eax, %%eax;"
				 "xor %%edx, %%edx;",
		VMI_OREG64 (ret),
		1, VMI_IREG1(VMI_CYCLES_STOLEN),
		VMI_CLOBBER(TWO_RETURNS));
	return ret;
}

static inline u64 vmi_get_wallclock(void)
{
	u64 ret;
	vmi_wrap_call(
		GetWallclockTime, "xor %%eax, %%eax;"
				  "xor %%edx, %%edx;",
		VMI_OREG64 (ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(TWO_RETURNS));
	return ret;
}

static inline int vmi_wallclock_updated(void)
{
	int ret;
	vmi_wrap_call(
		WallclockUpdated, "xor %%eax, %%eax;",
		VMI_OREG1 (ret),
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ONE_RETURN));
	return ret;
}

static inline void vmi_set_alarm(u32 flags, u64 expiry, u64 period)
{
	vmi_wrap_call(
		SetAlarm, "",
		VMI_NO_OUTPUT,
		5, VMI_XCONC(VMI_IREG1(flags),
			 VMI_IREG2((u32)expiry), VMI_IREG3((u32)(expiry >> 32)),
			 VMI_IREG4((u32)period), VMI_IREG5((u32)(period >> 32))),
		VMI_CLOBBER(ZERO_RETURNS));
}

static inline void vmi_cancel_alarm(u32 flags)
{
	vmi_wrap_call(
		CancelAlarm, "",
		VMI_NO_OUTPUT,
		1, VMI_IREG1(flags),
		VMI_CLOBBER(ONE_RETURN));
}

#endif
