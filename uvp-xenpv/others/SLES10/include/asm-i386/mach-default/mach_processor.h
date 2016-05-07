#ifndef _MACH_PROCESSOR_H
#define _MACH_PROCESSOR_H

/*
 * Generic CPUID function
 * clear %ecx since some cpus (Cyrix MII) do not set or clear %ecx
 * resulting in stale register contents being returned.
 */
static inline void cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
	__asm__("cpuid"
		: "=a" (*eax),
		  "=b" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (op), "c"(0));
}

/* Some CPUID calls want 'count' to be placed in ecx */
static inline void cpuid_count(int op, int count, int *eax, int *ebx, int *ecx,
	       	int *edx)
{
	__asm__("cpuid"
		: "=a" (*eax),
		  "=b" (*ebx),
		  "=c" (*ecx),
		  "=d" (*edx)
		: "0" (op), "c" (count));
}

/*
 * CPUID functions returning a single datum
 */
static inline unsigned int cpuid_eax(unsigned int op)
{
	unsigned int eax;

	__asm__("cpuid"
		: "=a" (eax)
		: "0" (op)
		: "bx", "cx", "dx");
	return eax;
}
static inline unsigned int cpuid_ebx(unsigned int op)
{
	unsigned int eax, ebx;

	__asm__("cpuid"
		: "=a" (eax), "=b" (ebx)
		: "0" (op)
		: "cx", "dx" );
	return ebx;
}
static inline unsigned int cpuid_ecx(unsigned int op)
{
	unsigned int eax, ecx;

	__asm__("cpuid"
		: "=a" (eax), "=c" (ecx)
		: "0" (op)
		: "bx", "dx" );
	return ecx;
}
static inline unsigned int cpuid_edx(unsigned int op)
{
	unsigned int eax, edx;

	__asm__("cpuid"
		: "=a" (eax), "=d" (edx)
		: "0" (op)
		: "bx", "cx");
	return edx;
}

#define load_cr3(pgdir) \
        asm volatile("movl %0,%%cr3": :"r" (__pa(pgdir)))

#define flush_deferred_cpu_state()
#define arch_update_kernel_stack(t,s)

/*
 * These special macros can be used to get or set a debugging register
 */
#define get_debugreg(var, register)				\
		__asm__("movl %%db" #register ", %0"		\
			:"=r" (var))
#define set_debugreg(value, register)				\
		__asm__("movl %0,%%db" #register		\
			: /* no output */			\
			:"r" (value))

/*
 * Set IOPL bits in EFLAGS from given mask
 */
static inline void set_iopl_mask(unsigned mask)
{
	unsigned int reg;
	__asm__ __volatile__ ("pushfl;"
			      "popl %0;"
			      "andl %1, %0;"
                              "orl %2, %0;"
			      "pushl %0;"
			      "popfl"
				: "=&r" (reg)
				: "i" (~X86_EFLAGS_IOPL), "r" (mask));
}

#endif
