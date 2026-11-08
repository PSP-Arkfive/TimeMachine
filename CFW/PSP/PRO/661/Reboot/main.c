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

PSP_MODULE_INFO("Reboot661PRO", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

RebootexConfigPRO conf;

int module_start(SceSize args, void *argp)
{
    conf.magic = REBOOTEX_CONFIG_MAGIC_PRO;
	conf.psp_model = sceKernelGetModel();
	conf.rebootex_size = size_rebootex;
	conf.psp_fw_version = sceKernelDevkitVersion();

    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    sctrlHENSetRebootexConfigOverride(&conf, sizeof(RebootexConfigPRO));

    return 0;
}

int module_stop(void)
{
    return 0;
}
