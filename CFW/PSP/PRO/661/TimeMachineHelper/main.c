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


char* pspTimeMachineGetPath(){
    return "ms0:/TM/661PRO";
}

int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    return 0;
}
