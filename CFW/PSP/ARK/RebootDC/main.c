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

PSP_MODULE_INFO("RebootDCARK", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);


RebootexConfigARK reboot_conf = {
    .magic = ARK_CONFIG_MAGIC
};


int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootbuffer_ms_dc_psp, size_rebootbuffer_ms_dc_psp);
    sctrlHENSetRebootexConfigOverride(&reboot_conf, sizeof(RebootexConfigARK));
    return 0;
}

int module_stop(void)
{
    return 0;
}
