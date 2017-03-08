###############################################################################
###### File name   : Makefile                                            ######
###### Author      :                                                     ######
###### Description : To optimize the PV_ON_HVM drivers's Makefile        ######
###### History     :                                                     ######
######               2012-02-24: Create the file.                        ######
######               2012-12-22: Makefile integrate with config.mk.      ######
###############################################################################

M=$(shell pwd)
COMPILEARGS = $(CROSSCOMPILE)
RHEL55_VBD = $(shell echo $(KERNDIR) | grep 2.6.18-194)
RHEL50_VBD = $(shell echo $(KERNDIR) | grep 2.6.18-8)
ZBQL_VBD = $(shell echo $(KERNDIR) | grep 2.6.18-SKL)
FEDORA6_VBD = $(shell echo $(KERNDIR) | grep 2.6.18-1.2798)
vbd_path=other

include $(M)/config.mk

obj-m += xen-platform-pci/
obj-m += xen-balloon/
obj-m += xen-vbd/
obj-m += xen-vnif/
obj-m += xen-scsi/
ifneq ($(RHEL55_VBD), )
vbd_path = 2.6.18-194
endif

ifneq ($(RHEL50_VBD), )
vbd_path = 2.6.18-8
endif

ifneq ($(ZBQL_VBD), )
vbd_path = 2.6.18-8
endif

ifneq ($(FEDORA6_VBD), )
vbd_path = 2.6.18-8
endif

all:lnfile
	make -C $(KERNDIR) M=$(M) modules $(COMPILEARGS)

modules_install:lnfile
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEARGS)

lnfile:
	@set -e; \
	cd xen-vbd; \
	for file in `ls $(vbd_path)`; \
	do \
		ln -sf $(vbd_path)/$$file $$file; \
	done; \
	cd -

clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEARGS)
	rm -rf Modules.symvers
