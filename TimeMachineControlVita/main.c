#include <stdio.h>
#include <string.h>

#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <pspkermit.h>
#include <psperror.h>
#include <pspkdebug.h>

#include <systemctrl.h>
#include <cfwmacros.h>
#include <bootloadex.h>

#include "flashloader.h"


PSP_MODULE_INFO("TimeMachine_Control_Vita", PSP_MODULE_KERNEL | PSP_MODULE_SINGLE_START | PSP_MODULE_SINGLE_LOAD | PSP_MODULE_NO_STOP, 1, 0);


#define BUF_SIZE 16*1024

static char package_path[256];
static char tm_helper_path[256];
static char satelite_path[256];
static char satelite_flash_path[256];
static STMOD_HANDLER previous;
static const char *tm_path;
static const char *satelite_name;
static const char *package_name;
static char buf[BUF_SIZE];


void ClearCaches()
{
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

static int extractSatelite(void)
{
    char filename[255];
    SceUID fdr = sceIoOpen(package_path, PSP_O_RDONLY, 0777);
    if (fdr < 0) return fdr;

    int filecount;
    sceIoRead(fdr, &filecount, sizeof(filecount));
    for (int i=0; i<filecount; i++){
        int filesize;
        sceIoRead(fdr, &filesize, sizeof(filesize));

        char namelen;
        sceIoRead(fdr, &namelen, sizeof(namelen));

        sceIoRead(fdr, filename, namelen);
        filename[namelen] = '\0';
        
        if (strstr(filename, satelite_name) != NULL){
            int fdw = sceIoOpen(satelite_path, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
            while (filesize>0){
                int read = sceIoRead(fdr, buf, (filesize>BUF_SIZE)?BUF_SIZE:filesize);
                sceIoWrite(fdw, buf, read);
                filesize -= read;
            }
            sceIoClose(fdw);
            return 0;
        }
        else {
            sceIoLseek(fdr, filesize, PSP_SEEK_CUR); // skip file
        }
    }
    sceIoClose(fdr);
    return -1;
}

SceUID (*sceKernelLoadModuleOrig)(const char *path, int flags, SceKernelLMOption *option) = NULL;
static SceUID sceKernelLoadModulePatched(const char *path, int flags, SceKernelLMOption *option)
{
    if (path && strcmp(path, satelite_flash_path) == 0) {
        int k1 = pspSdkSetK1(0);
        SceIoStat stat;
        int result = sceIoGetstat(satelite_path, &stat);
        if (result < 0) result = extractSatelite();
        pspSdkSetK1(k1);
        if (result >= 0) {
            path = satelite_path;
        }
    }

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
        patchKermitPeripheral(package_path, tm_helper_path);
        ClearCaches();
    }

    int previous_result = previous ? previous(mod) : 0;
    return result < 0 ? result : previous_result;
}

int module_start(SceSize args, void *argp)
{
    char *(*getTmPath)(void) = (void *)sctrlHENFindFunction("TimeMachine_Helper", "TimeMachineHelper", 0x7B4B5603);
    char *(*getSateliteName)(void) = (void *)sctrlHENFindFunction("TimeMachine_Helper", "TimeMachineHelper", 0x41B39517);
    char *(*getFlashPackageName)(void) = (void *)sctrlHENFindFunction("TimeMachine_Helper", "TimeMachineHelper", 0x3B81C581);
    if (!getTmPath || !getSateliteName || !getFlashPackageName) {
        Kprintf("TimeMachine: helper path exports not found\n");
        return SCE_EINVAL;
    }

    tm_path = getTmPath();
    satelite_name = getSateliteName();
    package_name = getFlashPackageName();
    if (!tm_path || !satelite_name || !package_name) {
        Kprintf("TimeMachine: helper returned an invalid path\n");
        return SCE_EINVAL;
    }

    int package_len = snprintf(package_path, sizeof(package_path), "%s/%s", tm_path, package_name);
    int helper_len = snprintf(tm_helper_path, sizeof(tm_helper_path), "%s/tmhlpr.prx", tm_path);
    int satelite_len = snprintf(satelite_path, sizeof(satelite_path), "%s/%s", tm_path, satelite_name);
    int flash_len = snprintf(satelite_flash_path, sizeof(satelite_flash_path),
                             "flash0:/vsh/module/%s", satelite_name);
    if (package_len < 0 || package_len >= sizeof(package_path) ||
        helper_len < 0 || helper_len >= sizeof(tm_helper_path) ||
        satelite_len < 0 || satelite_len >= sizeof(satelite_path) ||
        flash_len < 0 || flash_len >= sizeof(satelite_flash_path)) {
        Kprintf("TimeMachine: helper path is too long\n");
        return SCE_ENAMETOOLONG;
    }

    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    ClearCaches();

    return 0;
}
