/******************************************************************************
 * platform-pci-unplug.c
 *
 * Xen platform PCI device driver
 * Copyright (c) 2010, Citrix
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 */
 
#include <asm/io.h>

#include <linux/init.h>
#include <linux/module.h>

/******************************************************************************
 *
 * Interface for granting foreign access to page frames, and receiving
 * page-ownership transfers.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/version.h>

#define XEN_IOPORT_MAGIC_VAL 0x49d2
#define XEN_IOPORT_LINUX_PRODNUM 0xffff
#define XEN_IOPORT_LINUX_DRVVER  ((LINUX_VERSION_CODE << 8) + 0x0)


#define XEN_IOPORT_BASE 0x10

#define XEN_IOPORT_PLATFLAGS   (XEN_IOPORT_BASE + 0) /* 1 byte access (R/W) */
#define XEN_IOPORT_MAGIC       (XEN_IOPORT_BASE + 0) /* 2 byte access (R) */
#define XEN_IOPORT_UNPLUG      (XEN_IOPORT_BASE + 0) /* 2 byte access (W) */
#define XEN_IOPORT_DRVVER      (XEN_IOPORT_BASE + 0) /* 4 byte access (W) */

#define XEN_IOPORT_SYSLOG      (XEN_IOPORT_BASE + 2) /* 1 byte access (W) */
#define XEN_IOPORT_PROTOVER    (XEN_IOPORT_BASE + 2) /* 1 byte access (R) */
#define XEN_IOPORT_PRODNUM     (XEN_IOPORT_BASE + 2) /* 2 byte access (W) */

#define UNPLUG_ALL_IDE_DISKS 1
#define UNPLUG_ALL_NICS 2
#define UNPLUG_AUX_IDE_DISKS 4
#define UNPLUG_ALL 7
#define UNPLUG_IGNORE 8

int xen_platform_pci;
EXPORT_SYMBOL_GPL(xen_platform_pci);
static int xen_emul_unplug;

void xen_unplug_emulated_devices(void)
{
       /* If the version matches enable the Xen platform PCI driver.
        * Also enable the Xen platform PCI driver if the version is really old
        * and the user told us to ignore it. */
       xen_platform_pci = 1;
       xen_emul_unplug |= UNPLUG_ALL_NICS;
       xen_emul_unplug |= UNPLUG_ALL_IDE_DISKS;

       /* Set the default value of xen_emul_unplug depending on whether or
        * not the Xen PV frontends and the Xen platform PCI driver have
        * been compiled for this kernel (modules or built-in are both OK). */
       if (xen_platform_pci && !xen_emul_unplug) {
#if (defined(CONFIG_XEN_NETDEV_FRONTEND) || \
               defined(CONFIG_XEN_NETDEV_FRONTEND_MODULE)) && \
               (defined(CONFIG_XEN_PLATFORM_PCI) || \
                defined(CONFIG_XEN_PLATFORM_PCI_MODULE))
               printk(KERN_INFO "Netfront and the Xen platform PCI driver have "
                               "been compiled for this kernel: unplug emulated ICs.\n");
               xen_emul_unplug |= UNPLUG_ALL_NICS;
#endif
#if (defined(CONFIG_XEN_BLKDEV_FRONTEND) || \
               defined(CONFIG_XEN_BLKDEV_FRONTEND_MODULE)) && \
               (defined(CONFIG_XEN_PLATFORM_PCI) || \
                defined(CONFIG_XEN_PLATFORM_PCI_MODULE))
               printk(KERN_INFO "Blkfront and the Xen platform PCI driver have " 
                               "been compiled for this kernel: unplug emulated disks.\n"
                               "You might have to change the root device\n"
                               "from /dev/hd[a-d] to /dev/xvd[a-d]\n"
                               "in your root= kernel command line option\n");
               xen_emul_unplug |= UNPLUG_ALL_IDE_DISKS;
#endif
       }
       /* Now unplug the emulated devices */
       if (xen_platform_pci && !(xen_emul_unplug & UNPLUG_IGNORE))
               outw(xen_emul_unplug, XEN_IOPORT_UNPLUG);
}

