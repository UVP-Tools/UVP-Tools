###############################################################################
###### File name   : Makefile                                            ######
###### Author      : uvptools@huawei.com                                 ######
###### Description : For output xenbus user-space library of new         ######
######               linux OS release                                    ######
###### History     :                                                     ######
######               2012-05-23: Create the file.                        ######
######               2012-12-28: Add the KERNDIR parameter for build.    ######
###############################################################################

M = $(shell pwd)

include $(M)/config.mk

COMPILEFLAGS=$(CROSSCOMPILE)

vnif_path=other

ifeq ($(NO_PROCFS), )
obj-m += xen-procfs/
endif

ifeq ("2.6.32-131.0.15.el6.x86_64", "$(BUILDKERNEL)")
obj-m += xen-scsi/
endif

ifeq ("2.6.32-220.el6.x86_64", "$(BUILDKERNEL)")
obj-m += xen-scsi/
endif

ifeq ("2.6.32-279.el6.x86_64", "$(BUILDKERNEL)")
obj-m += xen-scsi/
endif

ifeq ("2.6.32-358.el6.x86_64", "$(BUILDKERNEL)")
obj-m += xen-scsi/
endif
ifeq ("Ubuntu", "$(OSDISTRIBUTION)")
#the 3th kernel version
ifeq ("38", "$(KERN_VER)")
obj-m += xen-balloon/
endif
endif

ifeq ("Debian", "$(OSDISTRIBUTION)")
ifeq ("2.6.32-5-amd64", "$(BUILDKERNEL)")
obj-m += xen-balloon/
endif
endif

ifeq ("Oracle", "$(OSDISTRIBUTION)")
ifeq ("","$(ORACLE65)")
ifneq ("39", "$(KERN_VER)")
obj-m += xen-vbd/
endif
ifneq ("", "$(ORACLE56)")
obj-m += xen-balloon/
endif
obj-m += xen-vnif/
endif
endif

ifeq ("redhat", "$(OSDISTRIBUTION)")
ifeq ($(OS_DIST_RHEL_7), )
obj-m += xen-balloon/
obj-m += xen-vbd/
obj-m += xen-vnif/
obj-m += vni_front/
endif
endif

ifeq ("39","$(KERN_VER)")
vnif_path=oracle6
endif

ifeq ("3.16.0-4-amd64", "$(BUILDKERNEL)")
obj-m += xen-vnif/
vnif_path=debian8
endif

ifeq ("4.0.4-301.fc22.x86_64", "$(BUILDKERNEL)")
obj-m += xen-vnif/
vnif_path=fedora22
endif

all:lnfile
	make -C $(KERNDIR) M=$(M) modules $(COMPILEFLAGS)

modules_install:lnfile
	make -C $(KERNDIR) M=$(M) modules_install ${COMPILEFLAGS}
lnfile:
	@set -e; \
	cd xen-vnif; \
	for file in `ls $(vnif_path)`; \
	do \
		ln -sf $(vnif_path)/$$file $$file; \
	done; \
	cd -

clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEFLAGS)
	rm -rf ./*/*.order

