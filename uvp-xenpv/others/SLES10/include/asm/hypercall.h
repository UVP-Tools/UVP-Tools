#ifndef __HYPERCALL_H__
#define __HYPERCALL_H__

#ifdef CONFIG_X86_64
#include "hypercall_64.h"
#else
#include "hypercall_32.h"
#endif

#endif /* __HYPERCALL_H__ */
