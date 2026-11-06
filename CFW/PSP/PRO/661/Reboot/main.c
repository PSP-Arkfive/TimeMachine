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

int (* decodeKL4E)(void * arg1, unsigned int arg2, void * arg3, unsigned int arg4) = NULL;

void build_rebootex_configure(void)
{
	RebootexConfigPRO *conf = (RebootexConfigPRO *)(REBOOTEX_CONFIG);

	conf->magic = REBOOTEX_CONFIG_MAGIC_PRO;
	conf->psp_model = sceKernelGetModel();
	conf->rebootex_size = size_rebootex;
	conf->psp_fw_version = sceKernelDevkitVersion();
}

int LoadReboot(void * arg1, unsigned int arg2, void * arg3, unsigned int arg4)
{
    //reset reboot flags
	memset((char *)REBOOTEX_CONFIG, 0, 0x100);
	memset((char *)REBOOTEX_CONFIG_ISO_PATH, 0, 256);

    build_rebootex_configure();

    sceKernelGzipDecompress((unsigned char *)REBOOTEX_TEXT, REBOOTEX_MAX_SIZE, rebootex, NULL);

    return decodeKL4E(arg1, arg2, arg3, arg4);
}

int module_start(SceSize args, void *argp)
{
    SceModule* mod = sceKernelFindModuleByName("sceLoadExec");
    decodeKL4E = (void*)(mod->text_addr);

    sctrlHENSetLoadRebootOverrideHandler(LoadReboot);

    return 0;
}

int module_stop(void)
{
    return 0;
}
