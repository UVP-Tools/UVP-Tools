/*
 * linux/include/asm-x86_64/timex.h
 *
 * x86-64 architecture timex specifications
 */
#ifndef _ASMx8664_TIMEX_H
#define _ASMx8664_TIMEX_H

#include <asm/8253pit.h>
#include <asm/msr.h>
#include <asm/vsyscall.h>
#include <asm/hpet.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <linux/compiler.h>

#define CLOCK_TICK_RATE	PIT_TICK_RATE	/* Underlying HZ */

typedef unsigned long long cycles_t;

static inline cycles_t get_cycles (void)
{
	unsigned long long ret;

	rdtscll(ret);
	return ret;
}

/*
 * Stop RDTSC speculation. This is needed when you need to use RDTSC
 * (or get_cycles or vread that possibly accesses the TSC) in a defined
 * code region.
 *
 * (Could use an alternative three way for this if there was one.)
 */
static inline void rdtsc_barrier(void)
{
       alternative(ASM_NOP3, "mfence", X86_FEATURE_MFENCE_RDTSC);
       alternative(ASM_NOP3, "lfence", X86_FEATURE_LFENCE_RDTSC);
}

/* Like get_cycles, but make sure the CPU is synchronized. */
static __always_inline cycles_t get_cycles_sync(void)
{
	unsigned long long ret;

	rdtsc_barrier();
	rdtscll(ret);
	rdtsc_barrier();

	return ret;
}

extern unsigned int cpu_khz;

extern int read_current_timer(unsigned long *timer_value);
#define ARCH_HAS_READ_CURRENT_TIMER	1

extern struct vxtime_data vxtime;

#endif
