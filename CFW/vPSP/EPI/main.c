#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>

#include <cfwmacros.h>
#include <systemctrl.h>
#include <systemctrl_epi.h>
#include <rebootexconfig.h>
#include <bootloadex.h>

#include "rebootex.h"

PSP_MODULE_INFO("Reboot661_EPINEPHINE", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

RebootexConfigEPI g_rebootex_config = {0};

int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    sctrlHENSetRebootexConfigOverride(&g_rebootex_config, sizeof(RebootexConfigEPI));
    return 0;
}

int module_stop(void)
{
    return 0;
}
