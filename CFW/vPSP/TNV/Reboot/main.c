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

#include <rebootex.h>

PSP_MODULE_INFO("RebootV661TNV2", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

#define FLASH0_TNV_PATH "ms0:/TM/V661TN/FLASH0.TN"

RebootexConfigTNCEF conf = {0};

int module_start(SceSize args, void *argp)
{
    SceUID fd = sceIoOpen(FLASH0_TNV_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return fd;

    int read_bytes = sceIoRead(fd, (void*)VITA_FLASH_EXTRA, MAX_FLASH0_SIZE);
    sceIoClose(fd);
    if (read_bytes < 0) return read_bytes;
    if (read_bytes == 0) return -1;

    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    sctrlHENSetRebootexConfigOverride(&conf, sizeof(RebootexConfigTNCEF));
    return 0;
}

int module_stop(void)
{
    return 0;
}
