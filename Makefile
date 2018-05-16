CROSS_COMPILE = 
#mipsel-openwrt-linux-uclibc-
#arm-linux-gnueabihf-

CPP = $(CROSS_COMPILE)g++

TARGET	= h264_rec 

DIR		= . ./sound ./video
INC		= -I. -I./sound -I./video -I./include
CFLAGS	= -g -Wall

OBJPATH	= .

ifeq ($(CROSS_COMPILE),arm-linux-gnueabihf-)
LDFLAGS := -lpthread  -L./lib_arm -lasound -L./lib_arm/faac -lfaac
else ifeq ($(CROSS_COMPILE),mipsel-openwrt-linux-uclibc-)
LDFLAGS := -lm -ldl -lrt -lpthread  -L./lib_mips -lasound -L./lib_mips/faac -lfaac
#export STAGING_DIR=/opt/toolchain-mt7688/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/:$STAGING_DIR
else
LDFLAGS := -lpthread  -L./lib -lasound -L./lib/faac -lfaac
endif

FILES	= $(foreach dir,$(DIR),$(wildcard $(dir)/*.cpp))

OBJS	= $(patsubst %.cpp,%.o,$(FILES))

all:$(OBJS) $(TARGET)

$(OBJS):%.o:%.cpp
	$(CPP) $(CFLAGS) $(INC) -c -o $(OBJPATH)/$(notdir $@) $< 

$(TARGET):$(OBJPATH)
	$(CPP) -o $@ $(OBJPATH)/*.o $(LDFLAGS)

clean:
	-rm $(OBJPATH)/*.o
	-rm $(TARGET)