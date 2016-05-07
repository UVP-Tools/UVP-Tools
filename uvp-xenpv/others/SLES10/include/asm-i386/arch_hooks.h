#ifndef _ASM_ARCH_HOOKS_H
#define _ASM_ARCH_HOOKS_H

#include <linux/interrupt.h>
#include <asm/acpi.h>
#include <asm/arch_hooks.h>

/*
 *	linux/include/asm/arch_hooks.h
 *
 *	define the architecture specific hooks 
 */

/* these aren't arch hooks, they are generic routines
 * that can be used by the hooks */
extern void init_ISA_irqs(void);
extern void apic_intr_init(void);
extern void smp_intr_init(void);
extern irqreturn_t timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);

/*
 * There are also some generic structures used by most architectures.
 *
 * IRQ0 is the default timer interrupt
 * IRQ2 is cascade interrupt to second interrupt controller
 */
extern struct irqaction irq0;
extern struct irqaction irq2;

static inline void legacy_intr_init_hook(void)
{
	if (!acpi_ioapic)
		setup_irq(2, &irq2);
}

static inline void default_timer_init(void)
{
	setup_irq(0, &irq0);
}

/* include to allow sub-arch override */
#include <mach_hooks.h>

/* these are the default hooks */

/**
 * pre_intr_init_hook - initialisation prior to setting up interrupt vectors
 *
 * Description:
 *	Perform any necessary interrupt initialisation prior to setting up
 *	the "ordinary" interrupt call gates.  For legacy reasons, the ISA
 *	interrupts should be initialised here if the machine emulates a PC
 *	in any way.
 **/
#ifndef pre_intr_init_hook
#define pre_intr_init_hook() init_ISA_irqs()
#endif


/**
 * intr_init_hook - post gate setup interrupt initialisation
 *
 * Description:
 *	Fill in any interrupts that may have been left out by the general
 *	init_IRQ() routine.  interrupts having to do with the machine rather
 *	than the devices on the I/O bus (like APIC interrupts in intel MP
 *	systems) are started here.
 **/
#ifndef intr_init_hook
#define intr_init_hook() legacy_intr_init_hook()
#endif


/**
 * pre_setup_arch_hook - hook called prior to any setup_arch() execution
 *
 * Description:
 *	generally used to activate any machine specific identification
 *	routines that may be needed before setup_arch() runs.  On VISWS
 *	this is used to get the board revision and type.
 **/
#ifndef pre_setup_arch_hook
#define pre_setup_arch_hook()
#endif


/**
 * post_setup_arch_hook - hook called after any setup_arch() execution
 *
 * Description:
 *      Provides a hook point immediate after setup_arch completes.
 **/
#ifndef post_setup_arch_hook
#define post_setup_arch_hook()
#endif


/**
 * trap_init_hook - initialise system specific traps
 *
 * Description:
 *	Called as the final act of trap_init().  Used in VISWS to initialise
 *	the various board specific APIC traps.
 **/
#ifndef trap_init_hook
#define trap_init_hook()
#endif


/**
 * time_init_hook - do any specific initialisations for the system timer.
 *
 * Description:
 *	Must plug the system timer interrupt source at HZ into the IRQ listed
 *	in irq_vectors.h:TIMER_IRQ
 **/
#ifndef time_init_hook
#define time_init_hook() default_timer_init()
#endif

/**
 * smpboot_startup_ipi_hook - hook to set AP state prior to startup IPI
 *
 * Description:
 *	Used in VMI to allow hypervisors to setup a known initial state on
 * 	coprocessors, rather than booting from real mode.
 **/
#ifndef smpboot_startup_ipi_hook
#define smpboot_startup_ipi_hook(...)
#endif


/**
 * module_finalize_hook - perform module fixups required by subarch
 *
 * Description:
 *	Used in VMI to apply annotations to kernel modules
 **/
#ifndef module_finalize_hook
#define module_finalize_hook(...)
#endif

#endif
