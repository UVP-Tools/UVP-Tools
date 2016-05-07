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

#ifndef _MACH_PGALLOC_H
#define _MACH_PGALLOC_H

#define mach_setup_pte(va, pfn)
#define mach_release_pte(va, pfn)

#define mach_setup_pmd(va, pfn)
#define mach_release_pmd(va, pfn)

#define mach_setup_pgd(va, pfn, root, base, pdirs)
#define mach_release_pgd(va, pfn)

#define mach_map_linear_pt(num, ptep, pfn)
#define mach_map_linear_range(start, pages, pfn)

#endif /* _MACH_PGALLOC_H */
