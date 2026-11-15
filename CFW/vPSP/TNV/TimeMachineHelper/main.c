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

#include <systemctrl.h>
#include <cfwmacros.h>
#include <bootloadex.h>

#include <rebootex.h>


PSP_MODULE_INFO("TimeMachine_Helper", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);

#define FLASH0_TNV_PATH "ms0:/TM/V661TN/FLASH0.TN"

static STMOD_HANDLER previous;

char* pspTimeMachineGetPath(){
    return "ms0:/TM/V661TN";
}

// kermit_peripheral's sub_000007CC clone, called by loadexec + 0x0000299C with a0=8 (was a0=7 for fw <210)
// Returns 0 on success
u64 kermit_flash_load(int cmd)
{
    u8 buf[128];
    u64 resp;
    void *alignedBuf = (void*)ALIGN_64((int)buf + 63);
    sceKernelDcacheInvalidateRange(alignedBuf, 0x40);
    SceKermitRequest *packet = (SceKermitRequest *)KERMIT_PACKET((int)alignedBuf);
    u32 argc = 0;
    sceKermitSendRequest(packet, KERMIT_MODE_PERIPHERAL, cmd, argc, KERMIT_CALLBACK_DISABLE, &resp);
    return resp;
}

int flashLoadPatch(int cmd)
{
    int ret = kermit_flash_load(cmd);
    // Custom handling on loadFlash mode, else nothing
    if ( cmd == KERMIT_CMD_ERROR_EXIT || cmd == KERMIT_CMD_ERROR_EXIT_2 )
    {
        // Wait for flash to load
        sceKernelDelayThread(10000);

        int fd = sceIoOpen(FLASH0_TNV_PATH, PSP_O_RDONLY, 0777);
        if (fd >= 0){
            sceIoRead(fd, (void*)VITA_FLASH_EXTRA, MAX_FLASH0_SIZE);
            sceIoClose(fd);
        }
        else {
            memset((void*)VITA_FLASH_EXTRA, 0, 32);
        }

        sctrlFlushCache();
    }
    return ret;
}

int patchKermitPeripheral()
{
    // Redirect KERMIT_CMD_ERROR_EXIT loadFlash function
    u32 swaddress = sctrlHENFindFirstJAL(sctrlHENFindFunction("sceKermitPeripheral_Driver", "sceKermitPeripheral_driver", 0x0648E1A3));
    REDIRECT_FUNCTION(swaddress, flashLoadPatch);
    return 0;
}

static SceUID sceKernelLoadModulePatched(const char *path, int flags, SceKernelLMOption *option)
{
    if (path && strcmp(path, "flash0:/vsh/module/satelite.prx") == 0)
        path = "ms0:/TM/V661TN/satelite.prx";

    return sceKernelLoadModule(path, flags, option);
}

static int OnModuleStart(SceModule *mod)
{
    if (strcmp(mod->modname, "VshControl") == 0) {
        int result = sctrlHookImportByNID(mod, "ModuleMgrForKernel", 0x977DE386, sceKernelLoadModulePatched);
        if (result < 0)
            printf("TimeMachine_Helper: failed to hook VshControl module loader: 0x%08X\n", result);
    }

    return previous ? previous(mod) : 0;
}

int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    return 0;
}
