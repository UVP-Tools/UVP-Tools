_XEN_CPPFLAGS += -I$(M)/include

OS_ARCH := $(shell uname -m)
KERN_VER := $(shell echo $(BUILDKERNEL) | awk -F'.' '{print $$3}' | awk -F'-' '{print $$1}')
DEBIAN6 := $(shell echo $(KERNDIR) | grep 2.6.32-5-amd64)
ORACLE65 := $(shell echo $(KERNDIR) | grep 3.8.13-16.2.1)
ORACLE56 := $(shell echo $(KERNDIR) | grep "2.6.32-[0-9]\{3\}\.")
OS_DIST_UBUNTU_KERNEL_3110 := $(shell echo $(KERNDIR) | grep 3.11.0)
OS_DIST_UBUNTU_KERNEL_3130 := $(shell echo $(KERNDIR) | grep "3.13.0\|3.16.0\|3.19.0\|4.2.3\|4.0.4")
OS_DIST_RHEL_7 := $(shell echo $(KERNDIR) | grep 3.10.0-123.el7)
OS_DIST_GENTOO_KERNEL_390 := $(shell echo $(OSDISTRIBUTION) | grep Gentoo)
#AUTOCONF := $(shell find $(CROSS_COMPILE_KERNELSOURCE) -name autoconf.h)

#debian8.2/ubuntu14.04.3/fedora22/23 not use xen_procfs
NO_PROCFS := $(shell echo $(KERNDIR) | grep "3.16.0-4\|3.19.0-25\|4.2.3-300\|4.0.4-301")

ifneq ($(NO_PROCFS), )
  _XEN_CPPFLAGS += -DNO_PROCFS
endif

OS_DIST_KERNEL_3190_UP := $(shell echo $(KERNDIR) | grep "3.19.0-25\|4.2.3-300\|4.0.4-301")
ifneq ($(OS_DIST_KERNEL_3190_UP), )
  _XEN_CPPFLAGS += -DKERNEL_3190_UP
endif


ifeq ("Oracle", "$(OSDISTRIBUTION)")
_XEN_CPPFLAGS += -DORACLE
endif

ifneq ($(OS_DIST_UBUNTU_KERNEL_3130), )
  _XEN_CPPFLAGS += -DUBUNTU_KERNEL_3110
endif

ifneq ($(OS_DIST_RHEL_7), )
  _XEN_CPPFLAGS += -DUBUNTU_KERNEL_3110
endif

ifneq ($(OS_DIST_UBUNTU_KERNEL_3110), )
_XEN_CPPFLAGS += -DUBUNTU_KERNEL_3110
endif

ifeq ("Gentoo","$(OSDISTRIBUTION)")
_XEN_CPPFLAGS += -DGENTOO_KERNEL_390
endif

#_XEN_CPPFLAGS += -include $(AUTOCONF)

EXTRA_CFLAGS += $(_XEN_CPPFLAGS)
EXTRA_AFLAGS += $(_XEN_CPPFLAGS)
CPPFLAGS := -I$(M)/include $(CPPFLAGS)
