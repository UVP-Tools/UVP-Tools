#ifndef MACH_HOOKS_H
#define MACH_HOOKS_H

#include <asm/desc.h>
#include <vmi_time.h>
#include <linux/elf.h>
#include <linux/module.h>

/**
 * pre_setup_arch_hook - hook called prior to any setup_arch() execution
 *
 * Description:
 *	generally used to activate any machine specific identification
 *	routines that may be needed before setup_arch() runs.
 *      We probe for various component option ROMs here.
 **/
extern void vmi_init(void);
#define pre_setup_arch_hook() vmi_init()

extern void vmi_module_finalize(const Elf_Ehdr *hdr,
			const Elf_Shdr *sechdrs,
			struct module *mod);
#define module_finalize_hook vmi_module_finalize

/**
 * smpboot_startup_ipi_hook - hook to set AP state prior to startup IPI
 *
 * Description:
 *	Used in VMI to allow hypervisors to setup a known initial state on
 * 	coprocessors, rather than booting from real mode.
 **/
extern void vmi_ap_start_of_day(int phys_apicid, unsigned long start_eip,
				unsigned long start_esp);
#define smpboot_startup_ipi_hook(apicid, eip, esp) \
	 vmi_ap_start_of_day(apicid, eip, esp)


#ifdef CONFIG_X86_LOCAL_APIC
extern fastcall void apic_vmi_timer_interrupt(void);
#endif

static inline void vmi_intr_init_hook(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	/* if the VMI-timer is used, redirect the local APIC timer interrupt
	 * gate to point to the vmi interrupt handler. */
	if (vmi_timer_used())
		set_intr_gate(LOCAL_TIMER_VECTOR, apic_vmi_timer_interrupt);
#endif
	legacy_intr_init_hook();
}
#define intr_init_hook() vmi_intr_init_hook()

/**
 * time_init_hook - do any specific initialisations for the system timer.
 *
 * Description:
 *	Must plug the system timer interrupt source at HZ into the IRQ listed
 *	in irq_vectors.h:TIMER_IRQ
 **/
extern struct irqaction vmi_irq0;
static inline void vmi_time_init_hook(void)
{
	if (vmi_timer_used())
		setup_irq(0, &vmi_irq0);
	else
		default_timer_init();
}
#define time_init_hook vmi_time_init_hook

#endif /* MACH_HOOKS_H */
