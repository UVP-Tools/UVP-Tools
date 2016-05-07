/*
 * Copyright (C) 2005, VMware, Inc.
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
 * Send feedback to zach@vmware.com
 *
 */

#ifndef __MACH_VMI_H
#define __MACH_VMI_H

/* Linux type system definitions */
#include <linux/types.h>
#include <paravirtualInterface.h>
#include <mach_asm.h>

/*
 * We must know the number of return values to avoid clobbering
 * one of EAX, EDX, which are used for 32-bit and 64-bit returns.
 */
#define VMI_CLOBBER_ZERO_RETURNS	"cc", "eax", "edx", "ecx"
#define VMI_CLOBBER_ONE_RETURN		"cc", "edx", "ecx"
#define VMI_CLOBBER_TWO_RETURNS		"cc", "ecx"
#define VMI_CLOBBER_FOUR_RETURNS	"cc"

#define VMI_CLOBBER(saved) VMI_XCONC(VMI_CLOBBER_##saved)
#define VMI_CLOBBER_EXTENDED(saved, extras...) VMI_XCONC(VMI_CLOBBER_##saved, extras)

/* Output register declarations */
#define VMI_OREG1 "=a"
#define VMI_OREG2 "=d"
#define VMI_OREG64 "=A"

/* Input register declarations */
#define VMI_IREG1 "a"
#define VMI_IREG2 "d"
#define VMI_IREG3 "c"
#define VMI_IREG4 "ir"
#define VMI_IREG5 "ir"

/*
 * Some native instructions (debug register access) need immediate encodings,
 * which can always be satisfied; surpress warnings in GCC 3.X by using 'V' as
 * a potential constraint.
 */
#if (__GNUC__ == 4)
#define VMI_IMM	  "i"
#else
#define VMI_IMM	  "iV"
#endif

/*
 * vmi_preambleX encodes the pushing of stack arguments;
 * Note the first three parameters are passed with
 * registers, thus there is no push.
 */
#define vmi_preamble0
#define vmi_preamble1
#define vmi_preamble2
#define vmi_preamble3
#define vmi_preamble4 "push %3;"
#define vmi_preamble5 "push %4; push %3;"

#define vmi_preamble(num_inputs) vmi_preamble##num_inputs

/*
 * vmi_postambleX encodes the after call stack fixup
 */
#define vmi_postamble0
#define vmi_postamble1
#define vmi_postamble2
#define vmi_postamble3
#define vmi_postamble4 "lea 4(%%esp), %%esp"
#define vmi_postamble5 "lea 8(%%esp), %%esp"

#define vmi_postamble(num_inputs) vmi_postamble##num_inputs

#define vmi_call_text(num_inputs)	\
      vmi_preamble(num_inputs)	"\n\t"	\
      XCSTR(vmi_callout)	"\n\t"	\
      vmi_postamble(num_inputs)	"\n\t"	\

/*
 * VMI inline assembly with input, output, and clobbers.
 * Multiple inputs and output must be wrapped using VMI_XCONC(...)
 */
#define vmi_call_lowlevel(call, native, alternative, output, input, clobber)	\
do {										\
	asm volatile (XCSTR(vmi_native_start) 				"\n\t"	\
		      native						"\n\t"	\
		      XCSTR(vmi_native_finish)				"\n\t"	\
										\
		      XCSTR(vmi_translation_start)			"\n\t"	\
		      alternative					"\n\t"	\
		      XCSTR(vmi_translation_finish)			"\n\t"	\
										\
		      XCSTR(vmi_padded_start) 				"\n\t"	\
		      native						"\n\t"	\
		      XCSTR(vmi_nop_pad)				"\n\t"	\
		      XCSTR(vmi_padded_finish)				"\n\t"	\
										\
		      :: input );						\
	asm volatile (XCSTR(vmi_annotate(%c0)) :: "i"(VMI_CALL_##call));	\
	asm volatile ( "" : output :: clobber );				\
} while (0)

#define vmi_wrap_call(call, native, output, num_inputs, input, clobber)		\
	vmi_call_lowlevel(call, native, vmi_call_text(num_inputs),		\
			  VMI_XCONC(output), VMI_XCONC(input), VMI_XCONC(clobber))

#define VMI_NO_INPUT
#define VMI_NO_OUTPUT

extern int hypervisor_found;

#endif /* __MACH_VMI_H */
