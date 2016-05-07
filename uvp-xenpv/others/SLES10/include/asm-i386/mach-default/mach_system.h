#ifndef _MACH_SYSTEM_H
#define _MACH_SYSTEM_H

#define clts() __asm__ __volatile__ ("clts")
#define read_cr0() ({ \
	unsigned int __dummy; \
	__asm__  __volatile__( \
		"movl %%cr0,%0\n\t" \
		:"=r" (__dummy)); \
	__dummy; \
})

#define write_cr0(x) \
	__asm__ __volatile__("movl %0,%%cr0": :"r" (x));

#define read_cr2() ({ \
        unsigned int __dummy; \
        __asm__  __volatile__( \
                "movl %%cr2,%0\n\t" \
                :"=r" (__dummy)); \
        __dummy; \
})
#define write_cr2(x) \
	__asm__  __volatile__("movl %0,%%cr2": :"r" (x));

#define read_cr3() ({ \
        unsigned int __dummy; \
        __asm__( \
                "movl %%cr3,%0\n\t" \
                :"=r" (__dummy)); \
        __dummy; \
})
#define write_cr3(x) \
	__asm__ __volatile__("movl %0,%%cr3": :"r" (x));

#define read_cr4() ({ \
	unsigned int __dummy; \
	__asm__( \
		"movl %%cr4,%0\n\t" \
		:"=r" (__dummy)); \
	__dummy; \
})

#define read_cr4_safe() ({			      \
	unsigned int __dummy;			      \
	/* This could fault if %cr4 does not exist */ \
	__asm__("1: movl %%cr4, %0		\n"   \
		"2:				\n"   \
		".section __ex_table,\"a\"	\n"   \
		".long 1b,2b			\n"   \
		".previous			\n"   \
		: "=r" (__dummy): "0" (0));	      \
	__dummy;				      \
})

#define write_cr4(x) \
	__asm__ __volatile__("movl %0,%%cr4": :"r" (x));

#define wbinvd() \
	__asm__ __volatile__ ("wbinvd": : :"memory");

/* interrupt control.. */
#define local_save_flags(x)     do { typecheck(unsigned long,x); __asm__ __volatile__("pushfl ; popl %0":"=g" (x): /* no input */); } while (0)

/* For spinlocks etc */
#define local_irq_restore(x)    do { typecheck(unsigned long,x); __asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"g" (x):"memory", "cc"); } while (0)

#define local_irq_disable()     __asm__ __volatile__("cli": : :"memory")
#define local_irq_enable()      __asm__ __volatile__("sti": : :"memory")

/* used in the idle loop; sti holds off interrupts for 1 instruction */
#define safe_halt()             __asm__ __volatile__("sti; hlt": : :"memory")

/* force shutdown of the processor; used when IRQs are disabled */
#define shutdown_halt()		__asm__ __volatile__("hlt": : :"memory")

/* halt until interrupted */
#define halt()			__asm__ __volatile__("hlt")

#endif
