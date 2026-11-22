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

#define TM_CFW "LME"
#define TM_INCLUDE_REBOOT_LOADER
#include "../../../Common/target_reboot.h"
PSP_MODULE_INFO("Reboot639LME", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

int LoadReboot(void * arg1, unsigned int arg2, void * arg3, unsigned int arg4)
{
    //reset reboot flags
	memset((char *)REBOOTEX_CONFIG, 0, sizeof(RebootexConfigLME));

    if (sceKernelGzipDecompress((unsigned char *)REBOOTEX_TEXT, REBOOTEX_MAX_SIZE, rebootex, NULL) < 0)
        return -1;

    return copyTargetReboot(arg1, arg2);
}

int module_start(SceSize args, void *argp)
{
    (void)size_rebootex;
    if (loadTargetReboot() < 0)
        return -1;

    sctrlHENSetLoadRebootOverrideHandler(LoadReboot);

    return 0;
}

int module_stop(void)
{
    return 0;
}
