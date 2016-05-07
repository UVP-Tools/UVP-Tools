#ifndef _MACH_ASM_IO_H

#include <vmi.h>

/*
 * Note on the VMI calls below; all of them must specify memory
 * clobber, even output calls.  The reason is that the memory
 * clobber is not an effect of the VMI call, but is used to
 * serialize memory writes by the compiler before an I/O
 * instruction.  In addition, even input operations may clobber
 * hardware mapped memory.
 */

static inline void vmi_outl(const u32 value, const u16 port)
{
	vmi_wrap_call(
		OUTL, "out %0, %w1",
		VMI_NO_OUTPUT,
		2, VMI_XCONC("a"(value), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void vmi_outb(const u8 value, const u16 port)
{
	vmi_wrap_call(
		OUTB, "outb %b0, %w1",
		VMI_NO_OUTPUT,
		2, VMI_XCONC("a"(value), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline void vmi_outw(const u16 value, const u16 port)
{
	vmi_wrap_call(
		OUTW, "outw %w0, %w1",
		VMI_NO_OUTPUT,
		2, VMI_XCONC("a"(value), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "memory"));
}

static inline u32 vmi_inl(const u16 port)
{
	u32 ret;
	vmi_wrap_call(
		INL, "in %w0, %%eax",
		VMI_OREG1(ret),
		1, VMI_XCONC("d"(port)),
		VMI_CLOBBER_EXTENDED(ONE_RETURN, "memory"));
	return ret;
}

static inline u8 vmi_inb(const u16 port)
{
	u8 ret;
	vmi_wrap_call(
		INB, "inb %w0, %%al",
		VMI_OREG1(ret),
		1, VMI_XCONC("d"(port)),
		VMI_CLOBBER_EXTENDED(ONE_RETURN, "memory"));
	return ret;
}

static inline u16 vmi_inw(const u16 port)
{
	u16 ret;
	vmi_wrap_call(
		INW, "inw %w0, %%ax",
		VMI_OREG1(ret),
		1, VMI_XCONC("d"(port)),
		VMI_CLOBBER_EXTENDED(ONE_RETURN, "memory"));
	return ret;
}

static inline void vmi_outsl(const void *addr, const u16 port, u32 count)
{
	vmi_wrap_call(
		OUTSL, "rep; outsl",
		VMI_NO_OUTPUT,
		3, VMI_XCONC("S"(addr), "c"(count), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "esi", "ecx", "memory"));
}

static inline void vmi_outsb(const void *addr, const u16 port, u32 count)
{
	vmi_wrap_call(
		OUTSB, "rep; outsb",
		VMI_NO_OUTPUT,
		3, VMI_XCONC("S"(addr), "c"(count), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "esi", "ecx", "memory"));
}

static inline void vmi_outsw(const void *addr, const u16 port, u32 count)
{
	vmi_wrap_call(
		OUTSW, "rep; outsw",
		VMI_NO_OUTPUT,
		3, VMI_XCONC("S"(addr), "c"(count), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "esi", "ecx", "memory"));
}

static inline void vmi_insl(const void *addr, const u16 port, u32 count)
{
	vmi_wrap_call(
		INSL, "rep; insl",
		VMI_NO_OUTPUT,
		3, VMI_XCONC("D"(addr), "c"(count), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "edi", "ecx", "memory"));
}

static inline void vmi_insb(const void *addr, const u16 port, u32 count)
{
	vmi_wrap_call(
		INSB, "rep; insb",
		VMI_NO_OUTPUT,
		3, VMI_XCONC("D"(addr), "c"(count), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "edi", "ecx", "memory"));
}

static inline void vmi_insw(const void *addr, const u16 port, u32 count)
{
	vmi_wrap_call(
		INSW, "rep; insw",
		VMI_NO_OUTPUT,
		3, VMI_XCONC("D"(addr), "c"(count), "d"(port)),
		VMI_CLOBBER_EXTENDED(ZERO_RETURNS, "edi", "ecx", "memory"));
}

/*
 * Allow I/O hypercalls to be turned off for debugging; trapping port I/O is
 * easier to log and rules out a potential source of errors during development.
 */
#ifdef CONFIG_VMI_IO_HYPERCALLS

#define __BUILDOUTINST(bwl,bw,value,port) \
	vmi_out##bwl(value, port)
#define	__BUILDININST(bwl,bw,value,port) \
	do { value = vmi_in##bwl(port); } while (0)
#define	__BUILDOUTSINST(bwl,addr,count,port) \
	vmi_outs##bwl(addr, port, count)
#define	__BUILDINSINST(bwl,addr,count,port) \
	vmi_ins##bwl(addr, port, count)

#else /* !CONFIG_VMI_IO_HYPERCALLS */

#include <../mach-default/mach_io.h>
#undef __SLOW_DOWN_IO

#endif


/*
 * Slow down port I/O by issuing a write to a chipset scratch
 * register.  This is an easy way to slow down I/O regardless
 * of processor speed, but useless in a virtual machine.
 */
static inline void vmi_iodelay(void)
{
	vmi_wrap_call(
		IODelay, "outb %%al, $0x80",
		VMI_NO_OUTPUT,
		0, VMI_NO_INPUT,
		VMI_CLOBBER(ZERO_RETURNS));
}

#define __SLOW_DOWN_IO vmi_iodelay()

#define _MACH_ASM_IO_H  /* At the end, so we can pull in native fallbacks */
#endif
