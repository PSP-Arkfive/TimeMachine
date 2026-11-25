PSPSDK = $(shell psp-config --pspsdk-path)
TARGET = tmtype6decrypt
OBJS = main.o

CFLAGS = -O2 -G0 -Wall -fno-pic
PSP_FW_VERSION = 660
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
USE_KERNEL_LIBS = 1
LIBS = -lpspsemaphore_660
LDFLAGS = -nostartfiles

include $(PSPSDK)/lib/build.mak
