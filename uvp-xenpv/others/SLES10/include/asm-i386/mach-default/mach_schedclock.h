
/*
 * Machine specific sched_clock routines.
 */

#ifndef __ASM_MACH_SCHEDCLOCK_H
#define __ASM_MACH_SCHEDCLOCK_H

#include <asm/msr.h>

static inline unsigned long long sched_clock_cycles(void)
{
	unsigned long long cycles;
	rdtscll(cycles);
	return cycles;
}

#endif /* __ASM_MACH_SCHEDCLOCK_H */
