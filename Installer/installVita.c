#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>
#include <vlf.h>

extern char boot_path[];
int StartInstallProgress(char *status);
void SetInstallProgress(int value);

static const char *vita_dirs[] = {
    "V661ARK", "V661EPI", "V661TN"
};

static const char *vita_modules[] = {
    "/TM/tmvita.prx",
    "/TM/V661ARK/reboot.prx",
    "/TM/V661EPI/reboot.prx",
    "/TM/V661TN/reboot.prx",
    "/TM/V661TN/tmhlpr.prx"
};

static int ensureDirectory(const char *path)
{
    SceIoStat stat;
    if (sceIoGetstat(path, &stat) >= 0)
        return FIO_S_ISDIR(stat.st_mode) ? 0 : -1;
    return sceIoMkdir(path, 0777);
}

static int copyBytes(SceUID input, const char *path, int size, int progress_start, int progress_end)
{
    char buffer[16384];
    SceUID output = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (output < 0)
        return output;

    int result = 0;
    int total = size;
    while (size > 0) {
        int amount = size < (int)sizeof(buffer) ? size : (int)sizeof(buffer);
        int read = sceIoRead(input, buffer, amount);
        if (read <= 0) {
            result = read < 0 ? read : -1;
            break;
        }
        int written = sceIoWrite(output, buffer, read);
        if (written != read) {
            result = written < 0 ? written : -1;
            break;
        }
        size -= read;
        if (progress_end > progress_start)
            SetInstallProgress(progress_start + (int)((long long)(progress_end - progress_start) * (total - size) / total));
        scePowerTick(0);
    }
    int close_result = sceIoClose(output);
    if (result == 0 && close_result < 0)
        result = close_result;
    if (result < 0)
        sceIoRemove(path);
    return result;
}

static int installModules(void)
{
    char package_path[320];
    snprintf(package_path, sizeof(package_path), "%s/TM.PKG", boot_path);
    SceUID input = sceIoOpen(package_path, PSP_O_RDONLY, 0);
    if (input < 0)
        return input;

    unsigned int found = 0, completed = 0;
    int count;
    int result = -1;
    SceOff package_size = sceIoLseek(input, 0, PSP_SEEK_END);
    if (package_size < 0 || sceIoLseek(input, 0, PSP_SEEK_SET) != 0)
        goto done;
    if (sceIoRead(input, &count, sizeof(count)) != sizeof(count) || count < 0)
        goto done;

    for (int i = 0; i < count; i++) {
        int size;
        unsigned char name_length;
        char name[256];
        if (sceIoRead(input, &size, sizeof(size)) != sizeof(size) || size < 0 ||
            sceIoRead(input, &name_length, sizeof(name_length)) != sizeof(name_length) ||
            sceIoRead(input, name, name_length) != name_length)
            goto done;
        name[name_length] = '\0';
        SceOff position = sceIoLseek(input, 0, PSP_SEEK_CUR);
        if (position < 0 || position > package_size || size > package_size - position)
            goto done;

        unsigned int flag = 0;
        for (unsigned int j = 0; j < sizeof(vita_modules) / sizeof(vita_modules[0]); j++) {
            if (strcmp(name, vita_modules[j]) == 0) {
                flag = 1U << j;
                break;
            }
        }

        if (flag) {
            if (size == 0) {
                result = -1;
                goto done;
            }
            char destination[260];
            snprintf(destination, sizeof(destination), "ms0:%s", name);
            result = copyBytes(input, destination, size, 0, 0);
            if (result < 0)
                goto done;
            if (!(found & flag)) {
                found |= flag;
                SetInstallProgress(85 + 14 * ++completed /
                    (sizeof(vita_modules) / sizeof(vita_modules[0])));
            }
        } else if (sceIoLseek(input, size, PSP_SEEK_CUR) < 0) {
            result = -1;
            goto done;
        }
        result = -1;
    }
    result = found == (1U << (sizeof(vita_modules) / sizeof(vita_modules[0]))) - 1 ? 0 : -1;

done:
    sceIoClose(input);
    return result;
}

static int copyTNPackage(const char *destination)
{
    char source[320];
    snprintf(source, sizeof(source), "%s/FLASH0.TN", boot_path);
    SceUID input = sceIoOpen(source, PSP_O_RDONLY, 0);
    if (input < 0) {
        snprintf(source, sizeof(source), "ms0:/FLASH0.TN");
        input = sceIoOpen(source, PSP_O_RDONLY, 0);
    }
    if (input < 0)
        return input;

    SceOff size = sceIoLseek(input, 0, PSP_SEEK_END);
    int result = -1;
    if (size > 0 && size <= 0x7fffffff) {
        if (strcmp(source, destination) == 0)
            result = 0;
        else if (sceIoLseek(input, 0, PSP_SEEK_SET) == 0)
            result = copyBytes(input, destination, (int)size, 5, 85);
    }
    sceIoClose(input);
    return result;
}

void InstallVita(void)
{
    int text = StartInstallProgress("Installing Time Machine, please wait...");
    char path[128];
    const char *step = "creating the destination";
    int result = -1;

    if ((result = ensureDirectory("ms0:/TM")) < 0)
        goto done;
    for (unsigned int i = 0; i < sizeof(vita_dirs) / sizeof(vita_dirs[0]); i++) {
        snprintf(path, sizeof(path), "ms0:/TM/%s", vita_dirs[i]);
        if ((result = ensureDirectory(path)) < 0)
            goto done;
    }
    SetInstallProgress(5);
    step = "copying FLASH0.TN";
    if ((result = copyTNPackage("ms0:/TM/V661TN/FLASH0.TN")) < 0)
        goto done;
    SetInstallProgress(85);
    step = "installing modules from TM.PKG";
    result = installModules();

done:
    if (result < 0) {
        char message[128];
        snprintf(message, sizeof(message), "Failed %s (0x%08X).", step, (unsigned int)result);
        vlfGuiMessageDialog(message, 1);
    } else {
        SetInstallProgress(100);
        vlfGuiSetText(text, "Installation complete.");
        sceKernelDelayThread(2000000);
    }
}
