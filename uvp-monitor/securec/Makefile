PROJECT=libsecurec.so
#if you need a debug version library, use "-g" instead of "-s -DNDEBUG -O2".
CFLAG= -I include -Wall  -s -DNDEBUG -O2 
CXXFLAG= $(CFLAG)
CC=gcc
GCC=gcc
ARCH:=$(shell getconf LONG_BIT)

ifeq ($(ARCH), 64)
    CFLAG += -fPIC
    CXXFLAG += -fPIC
endif

SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

$(PROJECT):$(OBJECTS)
	@mkdir -p ../obj
	mkdir -p ../lib
	$(GCC)  -shared -o ../lib/$@ $(patsubst %.o,../obj/%.o,$^) $(CFLAG) 
	@echo "finish $(PROJECT)"
install: 
	cp ../lib/libsecurec.so /usr/lib64/libsecurec.so
	cp ../include/securec.h /usr/include/securec.h
	cp ../include/securectype.h /usr/include/securectype.h
	@echo "finish install"
.c.o:
	@mkdir -p ../obj
	$(GCC) -c $< $(CFLAG) -o ../obj/$(patsubst %.c,%.o,$<)

clean:
	rm -rf *.o ../obj ../lib $(PROJECT)

