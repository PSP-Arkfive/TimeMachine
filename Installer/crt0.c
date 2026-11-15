#include <stdio.h>
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <kubridge.h>
#include <vlf.h>

extern int app_main(int argc, char *argv[]);
extern char boot_path[256];

static void startup_error(const char *message, int error)
{
    pspDebugScreenInit();
    pspDebugScreenPrintf("%s: 0x%08X\n", message, error);
    sceKernelDelayThread(5000000);
    sceKernelExitGame();
}

static int start_library(const char *name, SceSize args, void *argp)
{
    char path[128];
    SceUID mod = kuKernelLoadModule(name, 0, NULL);
    if (mod < 0) {
        snprintf(path, sizeof(path), "ms0:/PSP/LIBS/%s", name);
        mod = kuKernelLoadModule(path, 0, NULL);
    }
    if (mod < 0) {
        snprintf(path, sizeof(path), "ef0:/PSP/LIBS/%s", name);
        mod = kuKernelLoadModule(path, 0, NULL);
    }
    if (mod >= 0)
        mod = sceKernelStartModule(mod, args, argp, NULL, NULL);
    return mod;
}

int start_thread(SceSize args, void *argp)
{
    const char *path = argp;
    char *slash;
    int result;
    if (!path || !args || !memchr(path, 0, args) || strlen(path) >= sizeof(boot_path)) {
        startup_error("Invalid installer path", -1);
        return sceKernelExitDeleteThread(-1);
    }
    strcpy(boot_path, path);
    slash = strrchr(boot_path, '/');
    if (!slash) {
        startup_error("Invalid installer path", -1);
        return sceKernelExitDeleteThread(-1);
    }
    *slash = 0;
    result = sceIoChdir(boot_path);
    if (result < 0) {
        startup_error("Cannot open installer directory", result);
        return sceKernelExitDeleteThread(result);
    }
    result = start_library("intraFont-vlf.prx", args, argp);
    if (result < 0) {
        startup_error("Cannot load intraFont-vlf.prx", result);
        return sceKernelExitDeleteThread(result);
    }
    result = start_library("vlf.prx", args, argp);
    if (result < 0) {
        startup_error("Cannot load vlf.prx", result);
        return sceKernelExitDeleteThread(result);
    }
    vlfGuiInit(-1, app_main);
    return sceKernelExitDeleteThread(0);
}

int module_start(SceSize args, void *argp)
{
    SceUID thread = sceKernelCreateThread("start_thread", start_thread, 0x10, 0x4000, 0, NULL);
    int result;
    if (thread < 0)
        return thread;
    result = sceKernelStartThread(thread, args, argp);
    if (result < 0) {
        sceKernelDeleteThread(thread);
        return result;
    }
    return 0;
}
