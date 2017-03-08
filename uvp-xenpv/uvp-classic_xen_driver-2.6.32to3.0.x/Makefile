###############################################################################
###### File name   : Makefile											######
###### Author	  :													    ######
###### Description : To optimize the PV_ON_HVM drivers's Makefile		######
###### History	 :													    ######
######              2012-06-4 : Create the file                          ######
###############################################################################

M=$(shell pwd)
-include $(M)/config.mk

COMPILEARGS = $(CROSSCOMPILE)

reboot=$(OSVERSION)/reboot.c
obj-m += xen-platform-pci/
obj-m += xen-balloon/
obj-m += xen-vbd/
obj-m += xen-vnif/
ifeq (2.6.32, $(OSVERSION))
ifeq (x86_64, $(KERNELARCH))
reboot=reboot_64.c
endif
endif

ifeq (3.0.58, $(OSVERSION))
OSVERSION=3.0.13
endif

ifeq (3.0.80, $(OSVERSION))
OSVERSION=3.0.13
endif

###vni_front for VSA
ifeq ("2.6.32.59-0.7-default", "$(BUILDKERNEL)")
obj-m += vni_front/
endif

ifeq ("3.0.13-0.27-default", "$(BUILDKERNEL)")
obj-m += vni_front/
endif

ifeq ("3.0.13-0.27-pae", "$(BUILDKERNEL)")
obj-m += vni_front/
endif

ifeq (3.0.76, $(OSVERSION))
obj-m += vni_front/
endif

all:lnfile rename_headers
	make -C $(KERNDIR) M=$(M) modules $(COMPILEARGS)

modules_install:lnfile rename_headers
	make -C $(KERNDIR) M=$(M) modules_install $(COMPILEARGS)
	mkinitrd

lnfile:
	@set -e; \
	for dir in $(obj-m); \
	do \
		cd $$dir; \
		for file in `ls $(OSVERSION)`; \
		do \
			ln -sf $(OSVERSION)/$$file $$file; \
		done; \
		cd -; \
	done; \
	cd xen-platform-pci/; \
	ln -sf $(reboot) reboot.c; \
	cd ..

rename_headers:
	@if [ -d "/lib/modules/$(shell uname -r)/source/include/linux/" ]; then \
		cd /lib/modules/$(shell uname -r)/source/include/linux/; \
	elif [ -d "/lib/modules/$(shell uname -r)/build/include/linux/" ]; then \
		cd /lib/modules/$(shell uname -r)/build/include/linux/; \
	else \
		exit 0; \
	fi; \
	rename virtio VIRTIO virtio* 1>/dev/null 2>&1; \
	cd -

clean:
	make -C $(KERNDIR) M=$(M) clean $(COMPILEARGS)
	@find ./ -name modules.order | xargs rm -f
	@rm -rf Module.symvers
	@cd $$M
	@find ./ -type l | xargs rm -f
	@if [ -d "/lib/modules/$(shell uname -r)/source/include/linux/" ]; then \
		cd /lib/modules/$(shell uname -r)/source/include/linux/; \
	elif [ -d "/lib/modules/$(shell uname -r)/build/include/linux/" ]; then \
		cd /lib/modules/$(shell uname -r)/build/include/linux/; \
	else \
		exit 0; \
	fi; \
	rename VIRTIO virtio VIRTIO* 1>/dev/null 2>&1; \
	cd -
