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

#define TM_CFW "PRO"
#define TM_INCLUDE_REBOOT_LOADER
#include "../../../Common/target_reboot.h"
PSP_MODULE_INFO("Reboot639PRO", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

RebootexConfigPRO conf;

static int LoadTargetReboot(void *destination, unsigned int capacity, void *source, unsigned int unknown)
{
    int size = copyTargetReboot(destination, capacity);
    if (size < 0)
        return size;
    memset((void *)REBOOTEX_CONFIG, 0, sizeof(conf));
    memcpy((void *)REBOOTEX_CONFIG, &conf, sizeof(conf));
    memset((void *)REBOOTEX_CONFIG_ISO_PATH, 0, REBOOTEX_CONFIG_ISO_PATH_MAXSIZE);
    if (sceKernelGzipDecompress((unsigned char *)REBOOTEX_TEXT, REBOOTEX_MAX_SIZE, rebootex, NULL) < 0)
        return -1;
    return size;
}

int module_start(SceSize args, void *argp)
{
    conf.magic = REBOOTEX_CONFIG_MAGIC_PRO;
	conf.psp_model = sceKernelGetModel();
	conf.rebootex_size = size_rebootex;
    conf.psp_fw_version = 0x06030910;
    if (loadTargetReboot() < 0)
        return -1;
    sctrlHENSetLoadRebootOverrideHandler(LoadTargetReboot);

    return 0;
}

int module_stop(void)
{
    return 0;
}
