#include <string.h>

#include <cfwmacros.h>
#include <systemctrl.h>
#include <systemctrl_se.h>
#include <bootloadex.h>
#include <bootloadex_ark.h>

#include <fat.h>
#include <syscon.h>


ARKConfig arkconf = {
    .magic = ARK_CONFIG_MAGIC,
    .arkpath = DEFAULT_ARK_PATH_DC, // default path for ARK files
    .exploit_id = "TimeMachine", // exploit name
    .launcher = {0},
    .exec_mode = PSP_ORIG, // run ARK in PSP mode
    .recovery = 0,
};

BootLoadExConfig bleconf = {
    .boot_type = TYPE_REBOOTEX,
    .boot_storage = MS_BOOT,
    .extra_io.psp_io = {
        .use_fatms371 = 0,
        .use_dc = 1,
        .tm_path = "/TM/DCARK",
        .FatMount = &MsFatMount,
        .FatOpen = &MsFatOpen,
        .FatRead = &MsFatRead,
        .FatClose = &MsFatClose,
    },
    .UnpackBootConfig = &UnpackBootConfigArkPSP,
};


// Entry Point
int cfwBoot(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7)
{

    // GPIO enable
    REG32(0xbc10007c) |= 0xc8;
    __asm("sync"::);
    
    syscon_init();
    syscon_ctrl_ms_power(1);

    // Configure ARK
    memcpy(ark_config, &arkconf, sizeof(ARKConfig));

    // Configure
    configureBoot(&bleconf);

    // scan functions
    findBootFunctions();
    
    // patch sceboot
    patchBootPSP();
    
    // Forward Call
    return sceBoot(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}
