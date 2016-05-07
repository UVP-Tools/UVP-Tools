#ifndef _MACH_ASM_IO_H
#define _MACH_ASM_IO_H

#ifdef SLOW_IO_BY_JUMPING
#define __SLOW_DOWN_IO asm volatile("jmp 1f; 1: jmp 1f; 1:")
#else
#define __SLOW_DOWN_IO asm volatile("outb %al, $0x80")
#endif

#define __BUILDOUTINST(bwl,bw,value,port) \
	__asm__ __volatile__("out" #bwl " %" #bw "0, %w1" : : "a"(value), "Nd"(port))
#define	__BUILDININST(bwl,bw,value,port) \
	__asm__ __volatile__("in" #bwl " %w1, %" #bw "0" : "=a"(value) : "Nd"(port))
#define	__BUILDOUTSINST(bwl,addr,count,port) \
	__asm__ __volatile__("rep; outs" #bwl : "+S"(addr), "+c"(count) : "d"(port))
#define	__BUILDINSINST(bwl,addr,count,port) \
	__asm__ __volatile__("rep; ins" #bwl \
			     : "+D"(addr), "+c"(count) \
			     : "d"(port) \
			     : "memory")

#endif
