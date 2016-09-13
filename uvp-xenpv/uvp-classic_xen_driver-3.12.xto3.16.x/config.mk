# Hack: we need to use the config which was used to build the kernel,
# except that that won't have the right headers etc., so duplicate
# some of the mach-xen infrastructure in here.
#
# (i.e. we need the native config for things like -mregparm, but
# a Xen kernel to find the right headers)
_XEN_CPPFLAGS += -D__XEN_INTERFACE_VERSION__=0x00030205
_XEN_CPPFLAGS += -DCONFIG_XEN_COMPAT=0xffffff
_XEN_CPPFLAGS += -I$(M)/include -I$(M)/compat-include -DHAVE_XEN_PLATFORM_COMPAT_H

OSVERSION = $(shell echo $(BUILDKERNEL) | awk -F"." '{print $$1 "." $$2 "." $$3}' | awk -F"-" '{print $$1}')

ifeq ($(ARCH),ia64)
  _XEN_CPPFLAGS += -DCONFIG_VMX_GUEST
endif

ifeq ("3.16.6", "$(OSVERSION)")
  _XEN_CPPFLAGS += -DOPENSUSE_1302
endif

VERSION := $(shell cat /etc/SuSE-release | grep VERSION | awk -F" " '{print $$3}')
PATCHLEVEL := $(shell cat /etc/SuSE-release | grep PATCHLEVEL | awk -F" " '{print $$3}')
OSTYPE := "SUSE$(VERSION)SP$(PATCHLEVEL)"

AUTOCONF := $(shell find $(CROSS_COMPILE_KERNELSOURCE) -name autoconf.h)

_XEN_CPPFLAGS += -include $(AUTOCONF)

EXTRA_CFLAGS += $(_XEN_CPPFLAGS)
EXTRA_AFLAGS += $(_XEN_CPPFLAGS)
CPPFLAGS := -I$(M)/include $(CPPFLAGS)
