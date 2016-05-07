/*
 * Basic functions accessing APICs.
 * These may optionally be overridden by the subarchitecture in
 * mach_apicops.h.
 */

#ifndef __ASM_MACH_APICOPS_H
#define __ASM_MACH_APICOPS_H


#ifdef CONFIG_X86_LOCAL_APIC

static __inline void apic_write(unsigned long reg, unsigned long v)
{
	*((volatile unsigned long *)(APIC_BASE+reg)) = v;
}

static __inline void apic_write_atomic(unsigned long reg, unsigned long v)
{
	xchg((volatile unsigned long *)(APIC_BASE+reg), v);
}
static __inline unsigned long apic_read(unsigned long reg)
{
	return *((volatile unsigned long *)(APIC_BASE+reg));
}

#endif /* CONFIG_X86_LOCAL_APIC */

#endif /* __ASM_MACH_APICOPS_H */
