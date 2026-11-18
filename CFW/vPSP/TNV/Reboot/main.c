#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pspkernel.h>
#include <pspkermit.h>
#include <psperror.h>
#include <psputilsforkernel.h>

#include <cfwmacros.h>
#include <systemctrl.h>
#include <systemctrl_ark.h>
#include <rebootexconfig.h>
#include <bootloadex.h>

#include <rebootex.h>
#include "../flash_loader.h"


PSP_MODULE_INFO("RebootV661TNV2", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

RebootexConfigTNCEF conf = {0};

int module_start(SceSize args, void *argp)
{
    SceIoStat stat, helper_stat;
    int res = sceIoGetstat(FLASH0_TNV_PATH, &stat);
    if (res < 0) return res;

    res = sceIoGetstat(TMHLPR_PATH, &helper_stat);
    if (res < 0) return res;

    if (helper_stat.st_size <= 0 || helper_stat.st_size > MAX_FLASH0_SIZE - TMHLPR_ENTRY_OVERHEAD
        || stat.st_size < 4
        || stat.st_size > MAX_FLASH0_SIZE - TMHLPR_ENTRY_OVERHEAD - helper_stat.st_size)
        return SCE_KERR_ILLEGAL_SIZE;

    res = patchKermitPeripheral();
    if (res < 0) return res;

    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    sctrlHENSetRebootexConfigOverride(&conf, sizeof(RebootexConfigTNCEF));

    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();

    return 0;
}

int module_stop(void)
{
    return 0;
}
