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
    sctrlHENSetRebootexOverride(rebootbuffer_ms_dc_psp, size_rebootbuffer_ms_dc_psp);
    sctrlHENSetRebootexConfigOverride(&reboot_conf, sizeof(RebootexConfigARK));
    memcpy((void*)ARK_CONFIG, &arkconf, sizeof(ARKConfig));
    return 0;
}

int module_stop(void)
{
    return 0;
}
