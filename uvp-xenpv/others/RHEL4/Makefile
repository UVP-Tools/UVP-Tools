###############################################################################
###### File name   : Makefile                                            ######
###### Author      :                                                     ######
###### Description : To optimize the PV_ON_HVM drivers's Makefile        ######
###### History     :                                                     ######
######               2012-02-24 : Create the file                        ######
###############################################################################

M=$(shell pwd)
COMPILEARGS = $(CROSSCOMPILE)
ASIANUX_BALLOON = $(shell echo $(KERNDIR) | grep "2.6.9-89" | grep "AXS")

balloon_path=other
ifneq ($(ASIANUX_BALLOON),)
balloon_path = 2.6.9-89
endif
obj-m += xen-platform-pci/
obj-m += xen-balloon/
obj-m += xen-vbd/
obj-m += xen-vnif/
obj-m += xen-vmdq/
obj-m += xen-hcall/

all:lnfile
	make -C $(KERNDIR) M=$(M) modules $(COMPILEARGS)

modules_install:lnfile
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEARGS)
lnfile:
	@set -e; \
	cd xen-balloon; \
	for file in `ls $(balloon_path)`; \
	do \
		ln -sf $(balloon_path)/$$file $$file; \
	done; \
	cd -
clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEARGS)
	rm -rf Modules.symvers
