###############################################################################
###### File name   : Makefile                                            ######
###### Author      :                                                     ######
###### Modify      : Jinjian 00196667                                    ######
###### Description : To optimize the PV_ON_HVM drivers's Makefile        ######
###### History     :                                                     ######
######               2012-02-24 : Create the file                        ######
###############################################################################

M=$(shell pwd)
-include $(M)/config.mk
COMPILEARGS = $(CROSSCOMPILE)

obj-m += xen-platform-pci/
ifneq ("3.16.6", "$(OSVERSION)")
obj-m += xen-balloon/
obj-m += xen-scsi/
endif
obj-m += xen-vbd/
obj-m += xen-vnif/
obj-m += xen-hcall/

ifeq ("SUSE12SP1", $(OSTYPE))
OSVERSION=3.12.49
endif

ifeq ("SUSE12SP0", $(OSTYPE))
OSVERSION=3.12.28
endif

all:lnfile
	make -C $(KERNDIR) M=$(M) modules $(COMPILEARGS)

modules_install:
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEARGS)

lnfile:
	@set -e; \
	for dir in $(obj-m); \
	do \
		if [ "xen-hcall/" = "$$dir" ] || [ "xen-balloon/" = "$$dir" ] || [ "xen-scsi/" = "$$dir" ]; then \
			continue; \
		fi; \
		if [ "xen-vbd/" = "$$dir" ] && [ "3.16.6" = "$(OSVERSION)" ]; then \
			cd $$dir; \
			for file in `ls opensuse`; \
				do \
					ln -sf opensuse/$$file $$file; \
				done; \
			cd -; \
			continue; \
		fi; \
		cd $$dir; \
		for file in `ls $(OSVERSION)`; \
		do \
			ln -sf $(OSVERSION)/$$file $$file; \
		done; \
		cd -; \
	done; \

clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEARGS)
	find ./ -name modules.order | xargs rm -f
	rm -rf Module.symvers
	cd $$M
	find ./ -type l | xargs rm -f
