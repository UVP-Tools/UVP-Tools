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


#ifndef __MACH_SEGMENT_H
#define __MACH_SEGMENT_H

#if !defined(CONFIG_X86_VMI)
# error invalid sub-arch include
#endif

#ifndef __ASSEMBLY__
static inline unsigned get_kernel_rpl(void)
{
	unsigned cs;
	__asm__ ("movl %%cs,%0" : "=r"(cs):);
	return cs & SEGMENT_RPL_MASK;
}
#endif

#define COMPARE_SEGMENT_STACK(segment, offset)	\
	pushl %eax;				\
	mov   offset+4(%esp), %eax;		\
	andl  $~SEGMENT_RPL_MASK, %eax;		\
	cmpw  $segment,%ax;			\
	popl  %eax;

#define COMPARE_SEGMENT_REG(segment, reg)	\
	pushl %eax;				\
	mov   reg, %eax;			\
	andl  $~SEGMENT_RPL_MASK, %eax;		\
	cmpw  $segment,%ax;			\
	popl  %eax;

#endif
