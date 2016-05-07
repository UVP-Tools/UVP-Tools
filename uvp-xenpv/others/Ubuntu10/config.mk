# Hack: we need to use the config which was used to build the kernel,
# except that that won't have the right headers etc., so duplicate
# some of the mach-xen infrastructure in here.
#
# (i.e. we need the native config for things like -mregparm, but
# a Xen kernel to find the right headers)
_XEN_CPPFLAGS += -D__XEN_INTERFACE_VERSION__=0x00030205
_XEN_CPPFLAGS += -DCONFIG_XEN_COMPAT=0xffffff
_XEN_CPPFLAGS += -I$(M)/include -I$(M)/compat-include -DHAVE_XEN_PLATFORM_COMPAT_H

OS_UBUNTU_1004 := $(shell echo $(KERNDIR) | grep "2.6.32.21\|2.6.32.38\|2.6.32-24\|2.6.32-28\|2.6.32-33")
OS_UBUNTU_9 := $(shell echo $(KERNDIR) | grep 2.6.31-14)

# for suse 10 sp2(2.6.29-bigsmp)
OS_DIST_2629 := $(shell echo $(KERNDIR) | grep '2.6.29-bigsmp')
# for debian 5
OS_DIST_DEBIAN := $(shell echo $(KERNDIR) | grep '2.6.26-2')
# for debian 6 32
OS_DIST_DEBIAN_6_32 := $(shell echo $(KERNDIR) | grep '2.6.32-5-686')

#AUTOCONF := $(shell find $(CROSS_COMPILE_KERNELSOURCE) -name autoconf.h)

ifeq ("Ubuntu", "$(OSDISTRIBUTION)")
  _XEN_CPPFLAGS += -DUBUNTU
endif

ifneq ($(OS_UBUNTU_1004), )
  _XEN_CPPFLAGS += -DUBUNTU_1004
endif

ifneq ($(OS_UBUNTU_9), )
  _XEN_CPPFLAGS += -DUBUNTU_9
endif

ifneq ($(OS_DIST_2629), )
  _XEN_CPPFLAGS += -DSUSE_2629
endif

ifneq ($(OS_DIST_DEBIAN), )
  _XEN_CPPFLAGS += -DDEBIAN5
endif

ifneq ($(OS_DIST_DEBIAN_6_32), )
  _XEN_CPPFLAGS += -DDEBIAN6_32
endif

ifeq ($(ARCH),ia64)
  _XEN_CPPFLAGS += -DCONFIG_VMX_GUEST
endif

# _XEN_CPPFLAGS += -include $(AUTOCONF)

EXTRA_CFLAGS += $(_XEN_CPPFLAGS)
EXTRA_AFLAGS += $(_XEN_CPPFLAGS)
CPPFLAGS := -I$(M)/include $(CPPFLAGS)
