PSPSDK=$(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config --pspdev-path)
CFWSDK = $(PSPDEV)/share/psp-cfw-sdk

TARGET = tminstaller
OBJS = \
	crt0.o \
	main.o \


CFLAGS = -std=c99 -O0 -G0 -g -Wall -fshort-wchar -fno-pic -mno-check-zero-division 
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c

LDFLAGS = -nostartfiles
LIBS = -lpspgum -lpspkubridge -lpsppower -lvlfgui -lvlfgu -lvlfutils -lpspdecrypt -lpsppsar -lpspuser -lvlflibc
PSP_FW_VERSION = 660

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Time Machine
PSP_EBOOT_PIC1 = ../Resources/PIC1.PNG
BUILD_PRX = 1
PSP_EF_AWARE=1

all: $(TARGET).prx

include $(PSPSDK)/lib/build.mak
