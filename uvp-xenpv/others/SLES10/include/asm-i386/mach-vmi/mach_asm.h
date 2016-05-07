#ifndef __MACH_ASM_H
#define __MACH_ASM_H

/*
 * Take care to ensure that these labels do not overlap any in parent scope,
 * as the breakage can be difficult to detect.  I haven't found a flawless
 * way to avoid collision because local \@ can not be saved for a future
 * macro definitions, and we are stuck running cpp -traditional on entry.S
 *
 * To work around gas bugs, we must emit the native sequence here into a
 * separate section first to measure the length.  Some versions of gas have
 * difficulty resolving vmi_native_end - vmi_native_begin during evaluation
 * of an assembler conditional, which we use during the .rept directive
 * below to generate the nop padding -- Zach
 */

/* First, measure the native instruction sequence length */
#define vmi_native_start			\
	.pushsection .vmi.native,"",@nobits;	\
	771:;
#define vmi_native_finish			\
	772:;					\
	.popsection;
#define vmi_native_begin	771b
#define vmi_native_end		772b
#define vmi_native_len		(vmi_native_end - vmi_native_begin)

/* Now, measure and emit the vmi translation sequence */
#define vmi_translation_start			\
	.pushsection .vmi.translation,"ax";	\
	781:;
#define vmi_translation_finish			\
	782:;					\
	.popsection;
#define vmi_translation_begin	781b
#define vmi_translation_end	782b
#define vmi_translation_len	(vmi_translation_end - vmi_translation_begin)

/* Finally, emit the padded native sequence */
#define vmi_padded_start				\
	791:;
#define vmi_padded_finish				\
	792:;
#define vmi_padded_begin	791b
#define vmi_padded_end		792b
#define vmi_padded_len		(vmi_padded_end - vmi_padded_begin)

/* Measure the offset of the call instruction in the translation */
#define vmi_callout		761: call .+5;
#define vmi_call_eip		761b
#define vmi_call_offset		(vmi_call_eip - vmi_translation_begin)

/*
 * Pad out the current native instruction sequence with a series of nops;
 * the nop used is 0x66* 0x90 which is data16 nop or xchg %ax, %ax.  We
 * pad out with up to 11 bytes of prefix at a time because beyond that
 * both AMD and Intel processors start to show inefficiencies.
 */
#define MNEM_OPSIZE		0x66
#define MNEM_NOP		0x90
#define VMI_NOP_MAX		12

#define vmi_nop_pad						\
.equ vmi_pad_total, vmi_translation_len - vmi_native_len;	\
.equ vmi_pad, vmi_pad_total;					\
.rept (vmi_pad+VMI_NOP_MAX-1)/VMI_NOP_MAX;			\
	.if vmi_pad > VMI_NOP_MAX;				\
		.equ vmi_cur_pad, VMI_NOP_MAX;			\
	.else;							\
		.equ vmi_cur_pad, vmi_pad;			\
	.endif;							\
	.if vmi_cur_pad > 1;					\
		.fill vmi_cur_pad-1, 1, MNEM_OPSIZE;		\
	.endif;							\
	.byte MNEM_NOP;						\
	.equ vmi_pad, vmi_pad - vmi_cur_pad;			\
.endr;

/*
 * Create an annotation for a VMI call; the VMI call currently must be
 * wrapped in one of the vmi_raw_call (for assembler) or one of the
 * family of defined wrappers for C code.
 */
#define vmi_annotate(name)				\
	.pushsection .vmi.annotation,"a";		\
	.align 4;					\
	.long name;					\
	.long vmi_padded_begin;				\
	.long vmi_translation_begin;			\
	.byte vmi_padded_len;				\
	.byte vmi_translation_len;			\
	.byte vmi_pad_total;				\
	.byte vmi_call_offset;				\
	.popsection;

#ifndef __ASSEMBLY__
struct vmi_annotation {
	unsigned long	vmi_call;
	unsigned char 	*nativeEIP;
	unsigned char	*translationEIP;
	unsigned char	native_size;
	unsigned char	translation_size;
	signed char	nop_size;
	unsigned char	call_offset;
};

extern struct vmi_annotation __vmi_annotation[], __vmi_annotation_end[];
#endif

#define vmi_raw_call(name, native)			\
	vmi_native_start;				\
	native;						\
	vmi_native_finish;				\
							\
	vmi_translation_start;				\
	vmi_callout;					\
	vmi_translation_finish;				\
							\
	vmi_padded_start;				\
	native;						\
	vmi_nop_pad;					\
	vmi_padded_finish;				\
							\
	vmi_annotate(name);

#include <vmiCalls.h>
#ifdef __ASSEMBLY__
/*
 * Create VMI_CALL_FuncName definitions for assembly code using
 * equates; the C enumerations can not be used without propagating
 * them in some fashion, and rather the obfuscate asm-offsets.c, it
 * seems reasonable to confine this here.
 */
.equ VMI_CALL_CUR, 0;
#define VDEF(call)				\
	.equ VMI_CALL_/**/call, VMI_CALL_CUR;	\
	.equ VMI_CALL_CUR, VMI_CALL_CUR+1;
VMI_CALLS
#undef VDEF
#endif /* __ASSEMBLY__ */

#define IRET		vmi_raw_call(VMI_CALL_IRET,	iret)
#define CLI		vmi_raw_call(VMI_CALL_DisableInterrupts, cli)
#define STI		vmi_raw_call(VMI_CALL_EnableInterrupts,	sti)
#define STI_SYSEXIT	vmi_raw_call(VMI_CALL_SYSEXIT,	sti; sysexit)

/*
 * Due to the presence of "," in the instruction, and the use of
 * -traditional to compile entry.S, we can not use a macro to
 * encapsulate (mov %cr0, %eax); the full expansion must be
 * written.
 */
#define GET_CR0		vmi_native_start;		\
			mov %cr0, %eax;			\
			vmi_native_finish;		\
			vmi_translation_start;		\
			vmi_callout;			\
			vmi_translation_finish;		\
			vmi_padded_start;		\
			mov %cr0, %eax;			\
			vmi_nop_pad;			\
			vmi_padded_finish;		\
			vmi_annotate(VMI_CALL_GetCR0);

#ifndef __ASSEMBLY__
/*
 * Several handy macro definitions used to convert the raw assembler
 * definitions here into quoted strings for use in inline assembler
 * from C code.
 *
 * To convert the value of a defined token to a string, XSTR(x)
 * To concatenate multiple parameters separated by commas, VMI_XCONC()
 * To convert the value of a defined value with commas, XCSTR()
 *
 * These macros are incompatible with -traditional
 */
#define MAKESTR(x)              #x
#define XSTR(x)                 MAKESTR(x)
#define VMI_XCONC(args...)	args
#define CONCSTR(x...)		#x
#define XCSTR(x...)		CONCSTR(x)

/*
 * Create a typedef to define the VMI call enumeration.
 */
#define VDEF(call) VMI_CALL_##call,
typedef enum VMICall {
   VMI_CALLS
   NUM_VMI_CALLS
} VMICall;
#undef VDEF

/* Absolute Hack */
asm(".equ VMI_CALL_EnableInterrupts, 29;\n\t");
asm(".equ VMI_CALL_DisableInterrupts, 30;\n\t");

/*
 * Sti and Cli are special cases, used in raw inline assembler strings;
 * they do not need much of the machinery provided by C-code to do type
 * checking or push arguments onto the stack, which means we can simply
 * quote the assembler versions defined above rather than try to pry
 * apart the call sites which use these raw strings.
 */
#define CLI_STRING	XCSTR(CLI)
#define STI_STRING	XCSTR(STI)

#endif /* !__ASSEMBLY__ */

#endif /* __MACH_ASM_H */
