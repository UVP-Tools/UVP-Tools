# Hack: we need to use the config which was used to build the kernel,
# except that that won't have the right headers etc., so duplicate
# some of the mach-xen infrastructure in here.
#
# (i.e. we need the native config for things like -mregparm, but
# a Xen kernel to find the right headers)
#EXTRA_CFLAGS += -D__XEN_INTERFACE_VERSION__=0x00030205
#EXTRA_CFLAGS += -DCONFIG_XEN_COMPAT=0xffffff
#EXTRA_CFLAGS += -I$(M)/include -I$(M)/compat-include -DHAVE_XEN_PLATFORM_COMPAT_H
#ifeq ($(ARCH),ia64)
#  EXTRA_CFLAGS += -DCONFIG_VMX_GUEST
#endif
#
#EXTRA_CFLAGS += -include $(objtree)/include/linux/autoconf.h
#AUTOCONF := $(shell find $(CROSS_COMPILE_KERNELSOURCE) -name autoconf.h)
_XEN_CPPFLAGS += -D__XEN_INTERFACE_VERSION__=0x00030205
_XEN_CPPFLAGS += -DCONFIG_XEN_COMPAT=0xffffff
_XEN_CPPFLAGS += -I$(M)/include -I$(M)/compat-include -DHAVE_XEN_PLATFORM_COMPAT_H


#_XEN_CPPFLAGS += -include $(AUTOCONF)

EXTRA_CFLAGS += $(_XEN_CPPFLAGS)
EXTRA_AFLAGS += $(_XEN_CPPFLAGS)
CPPFLAGS := -I$(M)/include $(CPPFLAGS)
