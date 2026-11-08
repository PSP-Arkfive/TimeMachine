#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <pspkernel.h>
#include <pspsysevent.h>
#include <psputilsforkernel.h>
#include <pspthreadman_kernel.h>
#include <pspsysmem_kernel.h>

#include <systemctrl.h>
#include <cfwmacros.h>

#include <rebootex.h>

PSP_MODULE_INFO("TimeMachine_Helper", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);

#define SIGCHECK_PATCH_ADDR         0x00005994
#define SIGCHECK_FUNC_ADDR          0x00007824


char* pspTimeMachineGetPath(){
    return "ms0:/TM/661LME";
}

void ClearCaches()
{
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

static void fix_sigcheck()
{
	SceModule *mod = sceKernelFindModuleByName("sceLoaderCore");
	u32 text_addr = mod->text_addr;
	MAKE_CALL( text_addr + SIGCHECK_PATCH_ADDR, text_addr + SIGCHECK_FUNC_ADDR );
}

int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    fix_sigcheck();
    ClearCaches();
    return 0;
}
