# Hack: we need to use the config which was used to build the kernel,
# except that that won't have the right headers etc., so duplicate
# some of the mach-xen infrastructure in here.
#
# (i.e. we need the native config for things like -mregparm, but
# a Xen kernel to find the right headers)
_XEN_CPPFLAGS += -D__XEN_INTERFACE_VERSION__=0x00030205
_XEN_CPPFLAGS += -DCONFIG_XEN_COMPAT=0xffffff
_XEN_CPPFLAGS += -I$(M)/include -I$(M)/compat-include -DHAVE_XEN_PLATFORM_COMPAT_H

KERNELARCH= $(shell uname -p)
OSVERSION = $(shell echo $(BUILDKERNEL) | awk -F"." '{print $$1 "." $$2 "." $$3}' | awk -F"-" '{print $$1}')
VRMDIST = $(shell echo $(BUILDKERNEL) | grep "2.6.32.59-0.3-default\|2.6.32.59-0.7-default")

ifneq ($(VRMDIST),)
  _XEN_CPPFLAGS += -DVRM
endif

ifeq ($(ARCH),ia64)
  _XEN_CPPFLAGS += -DCONFIG_VMX_GUEST
endif


ifeq (3.0.76, $(OSVERSION))
  _XEN_CPPFLAGS += -DSUSE_1103
endif


EXTRA_CFLAGS += $(_XEN_CPPFLAGS)
EXTRA_AFLAGS += $(_XEN_CPPFLAGS)
CPPFLAGS := -I$(M)/include $(CPPFLAGS)
