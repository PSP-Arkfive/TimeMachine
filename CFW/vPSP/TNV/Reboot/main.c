#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pspkernel.h>
#include <pspkermit.h>
#include <psperror.h>
#include <pspkdebug.h>
#include <psputilsforkernel.h>

#include <cfwmacros.h>
#include <systemctrl.h>
#include <systemctrl_ark.h>
#include <rebootexconfig.h>
#include <bootloadex.h>

#include <rebootex.h>
#include "../../../../TimeMachineControlVita/flashloader.h"

#define FLASH0_TNV_PATH "ms0:/TM/V661TN/FLASH0.TN"
#define TMHLPR_PATH "ms0:/TM/V661TN/tmhlpr.prx"

PSP_MODULE_INFO("RebootV661TNV2", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

RebootexConfigTNCEF conf = {0};

int module_start(SceSize args, void *argp)
{
    int res = patchKermitPeripheral(FLASH0_TNV_PATH, TMHLPR_PATH);
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
