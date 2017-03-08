###############################################################################
###### File name   : Makefile                                            ######
###### Author      :                                                     ######
###### Description : To optimize the PV_ON_HVM drivers's Makefile        ######
###### History     :                                                     ######
######               2012-02-24 : Create the file                        ######
###############################################################################

M=$(shell pwd)

COMPILEARGS = $(CROSSCOMPILE)

obj-m += xen-platform-pci/
obj-m += xen-balloon/
obj-m += xen-vbd/
obj-m += xen-vnif/

all:
	make -C $(KERNDIR) M=$(M) modules $(COMPILEARGS)
modules_install:
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEARGS)
clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEARGS)
	rm -rf Modules.symvers
