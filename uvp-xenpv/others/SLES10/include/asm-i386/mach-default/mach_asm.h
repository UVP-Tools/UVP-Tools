#ifndef __MACH_ASM_H
#define __MACH_ASM_H

#define IRET		iret
#define CLI		cli
#define STI		sti
#define STI_SYSEXIT	sti; sysexit
#define GET_CR0		mov %cr0, %eax
#define WRMSR		wrmsr
#define RDMSR		rdmsr
#define CPUID		cpuid

#define CLI_STRING	"cli"
#define STI_STRING	"sti"

#endif /* __MACH_ASM_H */
