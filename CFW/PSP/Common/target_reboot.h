#ifndef TM_TARGET_REBOOT_H
#define TM_TARGET_REBOOT_H

#include <pspiofilemgr.h>

#define TM_STRINGIFY_INNER(value) #value
#define TM_STRINGIFY(value) TM_STRINGIFY_INNER(value)
#define TM_TARGET_DIR "/TM/" TM_STRINGIFY(TM_FIRMWARE) TM_CFW

#ifdef TM_INCLUDE_REBOOT_LOADER
static unsigned char target_reboot[0x20000] __attribute__((aligned(64)));
static int target_reboot_size;

static int loadTargetReboot(void)
{
    char path[80];
    int model = sceKernelGetModel();
    if (model < 0 || model > 10)
        return -1;
    snprintf(path, sizeof(path), "ms0:" TM_TARGET_DIR "/reboot_%02dg.bin", model + 1);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return fd;
    SceOff size = sceIoLseek(fd, 0, PSP_SEEK_END);
    int result = -1;
    if (size > 0 && size <= sizeof(target_reboot) &&
        sceIoLseek(fd, 0, PSP_SEEK_SET) == 0 &&
        sceIoRead(fd, target_reboot, size) == size) {
        target_reboot_size = size;
        result = 0;
    }
    if (sceIoClose(fd) < 0)
        result = -1;
    return result;
}

static int copyTargetReboot(void *destination, unsigned int capacity)
{
    if (!destination || target_reboot_size <= 0 ||
        (unsigned int)target_reboot_size > capacity)
        return -1;
    memcpy(destination, target_reboot, target_reboot_size);
    return target_reboot_size;
}
#endif

#endif
