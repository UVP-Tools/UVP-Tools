#ifndef _MACH_HOOKS_H
#define _MACH_HOOKS_H

/**
 * intr_init_hook - post gate setup interrupt initialisation
 *
 * Description:
 *	Fill in any interrupts that may have been left out by the general
 *	init_IRQ() routine.  interrupts having to do with the machine rather
 *	than the devices on the I/O bus (like APIC interrupts in intel MP
 *	systems) are started here.
 **/
static inline void voyager_intr_init_hook(void)
{
#ifdef CONFIG_SMP
	smp_intr_init();
#endif
	legacy_intr_init_hook();
}

#define intr_init_hook() voyager_intr_init_hook()


/**
 * pre_setup_arch_hook - hook called prior to any setup_arch() execution
 *
 * Description:
 *	generally used to activate any machine specific identification
 *	routines that may be needed before setup_arch() runs.
 *	Voyagers run their CPUs from independent clocks, so disable
 *	the TSC code because we can't sync them
 **/
#define pre_setup_arch_hook()	\
do { 				\
	tsc_disable = 1;	\
} while (0)

#endif
