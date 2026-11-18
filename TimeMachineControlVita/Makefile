PSPSDK=$(shell psp-config --pspsdk-path)
CFWSDK = $(PSPDEV)/share/psp-cfw-sdk
PY = $(shell which python3)

TARGET = tmvita

OBJS = \
	main.o \
	
all: $(TARGET).prx

main.o: flashloader.h

INCDIR = 
CFLAGS = -std=c99 -Os -G0 -Wall -fno-pic -fshort-wchar

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

LIBDIR = 
LDFLAGS = -nostartfiles
LIBS = -lpspsystemctrl_kernel -lpspkermit_driver

PSP_FW_VERSION = 661

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
