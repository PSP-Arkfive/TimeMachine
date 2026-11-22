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

#include "flashemu.h"

PSP_MODULE_INFO("TimeMachine_Control", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);

int psp_model = 0;
char *tm_path = NULL;

int SysEventHandler(int eventId, char *eventName, void *param, int *result);

PspSysEventHandler sysEventHandler =
{
    .size = sizeof(PspSysEventHandler),
    .name = "",
    .type_mask = 0x00FFFF00,
    .handler = SysEventHandler
};

extern SceUID flashemu_sema;
extern int msNotReady;

STMOD_HANDLER previous;

void ClearCaches()
{
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

int OnModuleStart(SceModule *mod)
{
    char *moduleName = mod->modname;

    if (strcmp(moduleName, "sceLflashFatfmt") == 0)
    {
        u32 funcAddr = sctrlHENFindFunction("sceLflashFatfmt", "LflashFatfmt", 0xb7a424a4); // sceLflashFatfmtStartFatfmt
        if (funcAddr)
        {
        	MAKE_DUMMY_FUNCTION_RETURN_0(funcAddr);
        	ClearCaches();
        }
    }
    else if (strcmp(moduleName, "sceMediaSync") == 0)
    {
        u32 address = mod->text_addr + 0x135c;
        if (!strncmp(tm_path, "ms0:/TM/620", 11) ||
            !strncmp(tm_path, "ms0:/TM/639", 11)) {
            static const u32 signature[] = {
                0x27bdfe60, 0x3c037361, 0x3c052f3a, 0x3c09002f,
                0x34686c66, 0x34a73068, 0x3526646b, 0x27a40170
            };
            address = findTargetFunction(mod, signature, sizeof(signature) / sizeof(signature[0]));
            if (!address)
                return -1;
        }
        MAKE_DUMMY_FUNCTION_RETURN_0(address);
        ClearCaches();
    }

    if (previous) return previous(mod);
    return 0;
}

int module_start(SceSize args, void *argp)
{
    char* (*getTmPath)() = (char* (*)())sctrlHENFindFunction("TimeMachine_Helper", "TimeMachineHelper", 0x7B4B5603);
    if (getTmPath == NULL) return -1;

    tm_path = getTmPath();
    psp_model = sceKernelGetModel();
    if (InstallFlashEmu() < 0)
        return -1;
    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    ClearCaches();

    return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
    PrepareRebootFlashEmu();
    sceKernelUnregisterSysEventHandler(&sysEventHandler);

    return 0;
}

int SysEventHandler(int eventId, char *eventName, void *param, int *result)
{
    if (eventId == 0x10009) // resume
        msNotReady = 1;
    return 0;
}
