###############################################################################
###### File name   : Makefile                                            ######
###### Author      :                                                     ######
###### Modify      : songjiangtao 00202546                               ######
###### Description : To optimize the PV_ON_HVM drivers's Makefile        ######
###### History     :                                                     ######
######               2012-02-24 : Create the file                        ######
###############################################################################

M=$(shell pwd)

COMPILEFLAGS=$(CROSSCOMPILE)

obj-m += xen-platform-pci/
obj-m += xen-balloon/
obj-m += xen-vbd/
obj-m += xen-vnif/

all:
	make -C $(KERNDIR) M=$(M) modules $(COMPILEFLAGS)

modules_install:
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEFLAGS)

clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEFLAGS)
	rm -rf Modules.symvers
