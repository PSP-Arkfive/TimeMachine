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
#include <systemctrl_ark.h>
#include <cfwmacros.h>

#include <payload.h>

PSP_MODULE_INFO("TimeMachine_Helper", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);


char* pspTimeMachineGetPath(){
    return ARK_DC_PATH;
}

int module_start(SceSize args, void *argp)
{
    if (size_rebootbuffer_ms_psp == 0) return -1;

    sctrlHENSetRebootexOverride(rebootbuffer_ms_psp, size_rebootbuffer_ms_psp);

    return 0;
}
