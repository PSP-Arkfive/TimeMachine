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


static STMOD_HANDLER previous;
static const char *tm_path;
static const char *satelite_name;
static const char *package_name;

void ClearCaches()
{
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

static int extractSatelite(const char *satelite_path)
{
    char filename[64];
    char package_path[64];
    snprintf(package_path, sizeof(package_path), "%s/%s", tm_path, package_name);
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
            SceUID memid = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "satelite", PSP_SMEM_Low, filesize, NULL);
            if (memid < 0) {
                sceIoClose(fdr);
                return memid;
            }

            void *data = sceKernelGetBlockHeadAddr(memid);
            sceIoRead(fdr, data, filesize);

            int fdw = fdw = sceIoOpen(satelite_path, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
            sceIoWrite(fdw, data, filesize);
            sceIoClose(fdw);

            sceKernelFreePartitionMemory(memid);
            sceIoClose(fdr);
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
    const char prefix[] = "flash0:/vsh/module/";
    if (path && strncmp(path, prefix, sizeof(prefix) - 1) == 0 &&
        strcmp(path + sizeof(prefix) - 1, satelite_name) == 0) {
        size_t satelite_len = strlen(tm_path) + strlen(satelite_name) + 2;
        if (satelite_len > 256) return SCE_ENAMETOOLONG;
        char satelite_path[satelite_len];
        snprintf(satelite_path, sizeof(satelite_path), "%s/%s", tm_path, satelite_name);
        int k1 = pspSdkSetK1(0);
        SceIoStat stat;
        int result = sceIoGetstat(satelite_path, &stat);
        if (result < 0) result = extractSatelite(satelite_path);
        pspSdkSetK1(k1);
        if (result >= 0) {
            return sceKernelLoadModuleOrig(satelite_path, flags, option);
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
        patchKermitPeripheral(tm_path, package_name);
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
        return SCE_EINVAL;
    }

    tm_path = getTmPath();
    satelite_name = getSateliteName();
    package_name = getFlashPackageName();

    previous = sctrlHENSetStartModuleHandler(OnModuleStart);
    ClearCaches();

    return 0;
}
