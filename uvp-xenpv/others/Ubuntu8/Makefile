###############################################################################
###### File name   : Makefile                                            ######
###### Author      :                                                     ######
###### Modify      : songjiangtao 00202546                               ######
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
obj-m += xen-vmdq/
obj-m += xen-hcall/

all:
	make -C $(KERNDIR) M=$(M) modules $(COMPILEARGS)

modules_install:
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEARGS)

clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEARGS)
	rm -rf Module.symvers
