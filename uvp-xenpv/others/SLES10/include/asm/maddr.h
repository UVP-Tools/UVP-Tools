#ifndef __MADDR_H__
#define __MADDR_H__

#ifdef CONFIG_X86_64
#include "maddr_64.h"
#else
#include "maddr_32.h"
#endif

#endif /* _MADDR_H */
