#include <pspkernel.h>

#include <systemctrl.h>

#include <rebootex.h>


PSP_MODULE_INFO("TimeMachine_Helper", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);

char* pspTimeMachineGetPath(){
    return "ms0:/TM/V661TN";
}

char* pspTimeMachineGetSateliteName(){
    return "satelite.prx";
}

char* pspTimeMachineGetFlashPackageName(){
    return "FLASH0.TN";
}

int module_start(SceSize args, void *argp)
{
    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    return 0;
}
