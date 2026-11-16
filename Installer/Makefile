PSPSDK=$(shell psp-config --pspsdk-path)
PSPDEV = $(shell psp-config --pspdev-path)
CFWSDK = $(PSPDEV)/share/psp-cfw-sdk

TARGET = tminstaller
OBJS = \
	crt0.o \
	extractFw.o \
	main.o \


CFLAGS = -std=c99 -O0 -G0 -g -Wall -fshort-wchar -fno-pic -mno-check-zero-division 
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS) -c


INCDIR = ./
LIBDIR = /usr/local/pspdev/psp/sdk/lib/

LDFLAGS = -nostartfiles
LIBS = -lvlfgui -lcolordebugger -lvlfgu -lpspkubridge -lvlflibc -lpspgum -lvlfutils -lpspdecrypt -lpsppsar -lpsppower 
PSP_FW_VERSION = 660

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = TM Installer
PSP_EBOOT_ICON = ../Resources/TM_ICON.PNG
#PSP_EBOOT_ICON1 = res/icon1.pmf
#PSP_EBOOT_SND0 = res/snd0.at3
PSP_EBOOT_PIC1 = ../Resources/TM_PIC1.PNG
BUILD_PRX = 1
PSP_EF_AWARE=1

all: $(TARGET).prx

include $(PSPSDK)/lib/build.mak
