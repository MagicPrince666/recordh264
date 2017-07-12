CROSS_COMPILE = mipsel-openwrt-linux-uclibc-

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CROSS_COMPILE)g++
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm

STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

export AS LD CC CPP AR NM
export STRIP OBJCOPY OBJDUMP

CFLAGS := -Wall -O2 -g
CFLAGS += -I./include -I./sound -I./video

ifeq ($(CROSS_COMPILE),arm-linux-gnueabihf-)
LDFLAGS := -lpthread  -L./lib_arm -lasound -L./lib_arm/faac -lfaac
else ifeq ($(CROSS_COMPILE),mipsel-openwrt-linux-uclibc-)
LDFLAGS := -lm -ldl -lrt -lpthread  -L./lib_mips -lasound -L./lib_mips/faac -lfaac
else
LDFLAGS := -lpthread  -L./lib -lasound -L./lib/faac -lfaac
endif

export CFLAGS LDFLAGS

TOPDIR := $(shell pwd)
export TOPDIR

TARGET := test


obj-y += thread.o 
obj-y += sound/
obj-y += video/


all : 
	make -C ./ -f $(TOPDIR)/Makefile.build
	$(CPP) -o $(TARGET) built-in.o $(LDFLAGS)

clean:
	rm -f $(shell find . -name "*.o")
	rm -f $(shell find . -name "*.d")
	rm -f $(TARGET)
	
