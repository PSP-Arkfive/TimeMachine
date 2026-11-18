#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <pspkernel.h>
#include <pspsysevent.h>
#include <psputilsforkernel.h>
#include <pspthreadman_kernel.h>
#include <pspsysmem_kernel.h>
#include <pspkermit.h>
#include <psperror.h>
#include <pspkdebug.h>

#include <systemctrl.h>
#include <cfwmacros.h>
#include <bootloadex.h>

#include <rebootex.h>
#include "../flash_loader.h"


PSP_MODULE_INFO("TimeMachine_Helper", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);

static STMOD_HANDLER previous;

char* pspTimeMachineGetPath(){
    return "ms0:/TM/V661TN";
}

void ClearCaches()
{
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

SceUID (*sceKernelLoadModuleOrig)(const char *path, int flags, SceKernelLMOption *option) = NULL;
static SceUID sceKernelLoadModulePatched(const char *path, int flags, SceKernelLMOption *option)
{
    if (path && strcmp(path, "flash0:/vsh/module/satelite.prx") == 0)
        path = "ms0:/TM/V661TN/satelite.prx";

    return sceKernelLoadModuleOrig(path, flags, option);
}

static int OnModuleStart(SceModule *mod)
{
    int result = 0;
    if (strcmp(mod->modname, "VshControl") == 0) {
        HIJACK_FUNCTION(K_EXTRACT_IMPORT(sceKernelLoadModule), sceKernelLoadModulePatched, sceKernelLoadModuleOrig);
        ClearCaches();
    }

    // Patch Kermit Peripheral Module to load flash0
    if(strcmp(mod->modname, "sceKermitPeripheral_Driver") == 0)
    {
        result = patchKermitPeripheral();
        if (result < 0) Kprintf("TimeMachine: Kermit patch failed: 0x%08X\n", result);
        ClearCaches();
    }

    int previous_result = previous ? previous(mod) : 0;
    return result < 0 ? result : previous_result;
}

int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    return 0;
}
