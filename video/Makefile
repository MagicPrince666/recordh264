CROSS_COMPILE = 
#mipsel-openwrt-linux-uclibc-
#arm-linux-gnueabihf-
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
CFLAGS += -I../include -I../sound

ifeq ($(CROSS_COMPILE),arm-linux-gnueabihf-)
LDFLAGS := -lpthread  #-L../lib_arm -lasound -L./lib_arm/faac -lfaac
else ifeq ($(CROSS_COMPILE),mipsel-openwrt-linux-uclibc-)
LDFLAGS := -lm -ldl -lrt -lpthread  
#export STAGING_DIR=/opt/toolchain-mt7688/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/:$STAGING_DIR
else
LDFLAGS := -lpthread  -L../lib -lasound
endif

export CFLAGS LDFLAGS

TOPDIR := $(shell pwd)
export TOPDIR


obj-y += H264_UVC_TestAP.o h264_xu_ctrls.o v4l2uvc.o


all : 
	make -C ./ -f $(TOPDIR)/Makefile.build
	$(CPP) -o $(TARGET) built-in.o $(LDFLAGS)


clean:
	rm -f $(shell find -name "*.o")
	rm -f $(TARGET)

distclean:
	rm -f $(shell find -name "*.o")
	rm -f $(shell find -name "*.d")
	rm -f $(TARGET)
	