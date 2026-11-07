.PHONY: all clean rebootex_payload
PSPSDK=$(shell psp-config --pspsdk-path)
CFWSDK = $(PSPDEV)/share/psp-cfw-sdk
PY = $(shell which python3)

TARGET = reboot

OBJS = main.o

all: rebootex_payload $(TARGET).prx

rebootex_payload:
	make -C rebootex

INCDIR = rebootex
CFLAGS = -std=c99 -Os -G0 -Wall -fno-pic -fshort-wchar

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

LIBDIR = 
LDFLAGS = -nostartfiles
LIBS = -lpspsystemctrl_kernel

PSP_FW_VERSION = 660

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

clean:
	rm -rf *.o *.elf *.prx exports.o exports.c
	make -C rebootex clean