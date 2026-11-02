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

#include <flashemu.h>
#include <rebootex.h>

PSP_MODULE_INFO("TimeMachine_Control", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);

#define SIGCHECK_PATCH_ADDR         0x00005994
#define SIGCHECK_FUNC_ADDR          0x00007824

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
        MAKE_DUMMY_FUNCTION_RETURN_0(mod->text_addr + 0x135c);
        ClearCaches();
    }

    if (previous)
        return previous(mod);
    return 0;
}

static void fix_sigcheck()
{
	SceModule *mod = sceKernelFindModuleByName("sceLoaderCore");
	u32 text_addr = mod->text_addr;
	MAKE_CALL( text_addr + SIGCHECK_PATCH_ADDR, text_addr + SIGCHECK_FUNC_ADDR );
}

int module_start(SceSize args, void *argp)
{
    if (size_rebootex == 0)
        return -1;

    InstallFlashEmu();
    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    fix_sigcheck();
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
