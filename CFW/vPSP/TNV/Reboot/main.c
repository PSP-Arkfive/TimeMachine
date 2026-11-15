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

PSP_MODULE_INFO("RebootV661TNV2", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD, 1, 0);

#define FLASH0_TNV_PATH "ms0:/TM/V661TN/FLASH0.TN"
#define TMHLPR_PATH "ms0:/TM/V661TN/tmhlpr.prx"

RebootexConfigTNCEF conf = {0};

int module_start(SceSize args, void *argp)
{
    SceUID fd = sceIoOpen(FLASH0_TNV_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return fd;

    int read_bytes = sceIoRead(fd, (void*)VITA_FLASH_EXTRA, MAX_FLASH0_SIZE);
    sceIoClose(fd);
    if (read_bytes < 0) return read_bytes;
    if (read_bytes == 0) return -1;

    SceUID tmhlpr_fd = sceIoOpen(TMHLPR_PATH, PSP_O_RDONLY, 0777);
    if (tmhlpr_fd < 0) return tmhlpr_fd;

    int tmhlpr_size = sceIoLseek(tmhlpr_fd, 0, PSP_SEEK_END);
    sceIoLseek(tmhlpr_fd, 0, PSP_SEEK_SET);

    SceUID tmhlpr_memid = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "tmhlpr", PSP_SMEM_Low, tmhlpr_size, NULL);
    void *tmhlpr_buf = sceKernelGetBlockHeadAddr(tmhlpr_memid);

    int tmhlpr_read_bytes = sceIoRead(tmhlpr_fd, (void*)tmhlpr_buf, tmhlpr_size);
    sceIoClose(tmhlpr_fd);
    if (tmhlpr_read_bytes < 0) return tmhlpr_read_bytes;
    if (tmhlpr_read_bytes == 0) return -1;

    conf.module_after = "/kd/init.prx";
    conf.buf = tmhlpr_buf;
    conf.size = tmhlpr_read_bytes;
    conf.flags = 0x000000EF;

    sctrlHENSetRebootexOverride(rebootex, size_rebootex);
    sctrlHENSetRebootexConfigOverride(&conf, sizeof(RebootexConfigTNCEF));

    return 0;
}

int module_stop(void)
{
    return 0;
}
