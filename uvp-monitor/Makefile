##############################################################################
#                   Copyright 2016, Huawei Tech. Co., Ltd.
# FileName  :   Makefile
# Desciption:   The main makefile to build monitor module for Linux Distr.
##############################################################################

TARGET := uvp-monitor
CC     := gcc

cpu_bit = $(shell getconf LONG_BIT)
ifeq ($(cpu_bit), 32)
	cpu_bit=32
else
	cpu_bit=64
endif

# for debug version, set DBG_FLAAG=y
DBG_FLAG:=  

INC_FLAGS += -Iinclude -Isecurec/include

CFLAGS += -lpthread
ifeq ($(DBG_FLAG), y)
	CFLAGS += -g
endif

# don't provide pv-upgrade ability to user-compiled-pv vm
CFLAGS += -DNOT_USE_PV_UPGRADE

${TARGET}-${cpu_bit}: securec_api has_xs patch_xs
	$(CC) -o $@ ${INC_FLAGS} main.c xenctlmon.c network.c netinfo.c memory.c cpuinfo.c xenstore_common.c hostname.c cpu_hotplug.c disk.c upgrade.c healthcheck.c check_kernel.c ${CFLAGS} libsecurec.a -L. -lxenstore 
	$(CC) -o $@-static ${INC_FLAGS} main.c xenctlmon.c network.c netinfo.c memory.c cpuinfo.c xenstore_common.c hostname.c cpu_hotplug.c disk.c upgrade.c healthcheck.c check_kernel.c ${CFLAGS} libsecurec.a -L. libxenstore.a -L.
	$(CC) -o arping iputils/arping.c
	$(CC) -o ndsend iputils/ndsend.c

has_xs:
	@if [ -d "xen-4.1.2/tools/xenstore" ]; then \
		echo "You have a xenstore directory."; \
		exit 0; \
	elif [ -f "xen-4.1.2.tar.gz" ]; then \
		echo "You have a xen-4.1.2 source tarball."; \
	else \
		wget http://bits.xensource.com/oss-xen/release/4.1.2/xen-4.1.2.tar.gz; \
		if [ "X1" = "X$$?" ]; then \
			echo "You must download xen-4.1.2 source tarball." \
			exit 1; \
		fi; \
	fi; \
	tar -xzf xen-4.1.2.tar.gz xen-4.1.2/tools/xenstore; \
	exit 0

patch_xs:
	@if [ -d "xen-4.1.2/tools/xenstore" ]; then \
		patch -N -p0 < xenstore_dev_xen_xenbus_enable.diff; \
		cd xen-4.1.2/tools/xenstore/; \
		make all; \
		install -m755 libxenstore.so.3.0.0 /usr/lib; \
		ln -sf /usr/lib/libxenstore.so.3.0.0 /usr/lib/libxenstore.so.3.0; \
		ln -sf /usr/lib/libxenstore.so.3.0 /usr/lib/libxenstore.so; \
		cd -; \
	fi

securec_api:
	@cd ./securec/src/; \
	make -f MakefileCustom securecstatic; \
	cp libsecurec.a ../../libsecurec.a; \
	cd -

install:
	install -m544 uvp-monitor-${cpu_bit} /usr/bin/
	install -m544 ./xen-4.1.2/tools/xenstore/libxenstore.so.3.0.0 /usr/lib
	@cd /usr/lib; \
	ln -sf libxenstore.so.3.0.0 libxenstore.so.3.0; \
	ln -sf libxenstore.so.3.0.0 libxenstore.so; \
	cd -
	install -m544 monitor/uvp-monitor /etc/init.d/
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc3.d/S99uvp-monitor
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc3.d/K99uvp-monitor
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc5.d/S99uvp-monitor
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc5.d/K99uvp-monitor

install_static:
	install -m544 uvp-monitor-${cpu_bit}-static /usr/bin/
	install -m544 monitor/uvp-monitor /etc/init.d/
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc3.d/S99uvp-monitor
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc3.d/K99uvp-monitor
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc5.d/S99uvp-monitor
	@ln -sf /etc/init.d/uvp-monitor /etc/rc.d/rc5.d/K99uvp-monitor

.PHONY: clean
clean:
	@cd ./securec/src; \
	make clean; \
	cd -
	@if [ -d "./xen-4.1.2/tools/xenstore" ]; then \
		cd ./xen-4.1.2/tools/xenstore; \
		make clean; \
		cd -; \
	fi
	rm -f libsecurec.a
	rm -f libxenstore.a
	rm -f $(TARGET)-* 
	rm -f *.o arping ndsend
