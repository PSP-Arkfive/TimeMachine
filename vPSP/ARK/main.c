#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>

#include <cfwmacros.h>
#include <systemctrl.h>
#include <systemctrl_ark.h>
#include <rebootexconfig.h>
#include <bootloadex.h>

#include "rebootex.h"

PSP_MODULE_INFO("Reboot661_VPSP_ARK", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

#define FLASH0_ARK_PATH "ms0:/PSP/SAVEDATA/ARK_01234/FLASH0.ARK"
#define FLASH0_ARK_ADDR 0x8BA00000
#define FLASH0_ARK_SIZE 0x40000

ARKConfig arkconf = {
    .magic = ARK_CONFIG_MAGIC,
    .arkpath = "ms0:/PSP/SAVEDATA/ARK_01234/", // default path for ARK files
    .exploit_id = "Adrenaline",
    .launcher = {0},
    .exec_mode = PSV_ADR, // run ARK in PSP mode
    .recovery = 0,
};

RebootexConfigARK reboot_conf = {
    .magic = ARK_CONFIG_MAGIC
};

int module_start(SceSize args, void *argp)
{
    SceUID fd = sceIoOpen(FLASH0_ARK_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return fd;

    int read_bytes = sceIoRead(fd, (void*)FLASH0_ARK_ADDR, FLASH0_ARK_SIZE);
    sceIoClose(fd);
    if (read_bytes < 0) return read_bytes;
    if (read_bytes == 0) return -1;

    sctrlHENSetRebootexOverride(rebootbuffer_pentazemin, size_rebootbuffer_pentazemin);
    sctrlHENSetRebootexConfigOverride(&reboot_conf, sizeof(RebootexConfigARK));
    memcpy((void*)ARK_CONFIG, &arkconf, sizeof(ARKConfig));
    return 0;
}

int module_stop(void)
{
    return 0;
}
