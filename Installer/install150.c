#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <psppower.h>
#include <pspdecrypt.h>
#include <psprebootex.h>
#include <systemctrl_ark.h>
#include <vlf.h>

extern char boot_path[];
void SetInstallProgress(int value);

static const char *ark150_files[] = {
    "/kd/ark_systemctrl150.prx",
    "/kd/ark_vshctrl150.prx",
    "/vsh/module/ark_satelite150.prx",
    "/vsh/module/ark_recovery150.prx",
    "/kd/pspbtcnf_game.txt",
    "/kd/pspbtcnf.txt",
    "/mainbinex.raw",
    "/msipl.raw"
};

static const char *tm150_files[] = {
    "/TM/tmctrl.prx",
    "/TM/DCARK/150/reboot150.prx",
    "/TM/DCARK/150/tmctrl150.prx"
};

static SceUID openInput(const char *name)
{
    char path[320];
    snprintf(path, sizeof(path), "%s/%s", boot_path, name);
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        snprintf(path, sizeof(path), "ms0:/%s", name);
        fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    }
    return fd;
}

static int copyBytes(SceUID input, const char *path, int size, u8 *buffer, int capacity)
{
    SceUID output = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (output < 0)
        return output;
    int result = 0;
    while (size > 0) {
        int read = sceIoRead(input, buffer, size < capacity ? size : capacity);
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
        scePowerTick(0);
    }
    int close_result = sceIoClose(output);
    if (result == 0 && close_result < 0)
        result = close_result;
    if (result < 0)
        sceIoRemove(path);
    return result;
}

static int extractFiles(const char *archive, const char *prefix,
                        const char *const *files, unsigned int file_count,
                        u8 *buffer, int capacity, int install)
{
    SceUID fd = openInput(archive);
    if (fd < 0)
        return fd;
    int result = -1;
    unsigned int found = 0;
    int count;
    SceOff total = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (total < 0 || sceIoLseek(fd, 0, PSP_SEEK_SET) != 0 ||
        sceIoRead(fd, &count, sizeof(count)) != sizeof(count) || count < 0)
        goto done;
    for (int i = 0; i < count; i++) {
        int size;
        unsigned char length;
        char name[256];
        if (sceIoRead(fd, &size, sizeof(size)) != sizeof(size) || size < 0 ||
            sceIoRead(fd, &length, sizeof(length)) != sizeof(length) ||
            sceIoRead(fd, name, length) != length)
            goto done;
        name[length] = '\0';
        SceOff position = sceIoLseek(fd, 0, PSP_SEEK_CUR);
        if (position < 0 || position > total || size > total - position)
            goto done;
        unsigned int index;
        for (index = 0; index < file_count; index++)
            if (strcmp(name, files[index]) == 0)
                break;
        if (index < file_count) {
            char destination[320];
            if (size == 0 || (found & (1U << index)) ||
                snprintf(destination, sizeof(destination), "%s%s", prefix, name) >= (int)sizeof(destination))
                goto done;
            if (install) {
                result = copyBytes(fd, destination, size, buffer, capacity);
                if (result < 0)
                    goto done;
            } else if (sceIoLseek(fd, size, PSP_SEEK_CUR) != position + size) {
                goto done;
            }
            found |= 1U << index;
        } else if (sceIoLseek(fd, size, PSP_SEEK_CUR) < 0) {
            result = -1;
            goto done;
        }
        result = -1;
    }
    result = found == (1U << file_count) - 1 ? 0 : -1;
done:
    sceIoClose(fd);
    return result;
}

static int readFile(const char *path, int offset, u8 *buffer, int capacity)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return fd;
    int result = -1;
    SceOff size = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (size >= offset && size - offset <= capacity &&
        sceIoLseek(fd, offset, PSP_SEEK_SET) == offset) {
        int read = sceIoRead(fd, buffer, size - offset);
        result = read == size - offset ? read : -1;
    }
    sceIoClose(fd);
    return result;
}

int getReboot(u8 *input, u8 *output, int size, int capacity, int decompress)
{
    int length = pspDecryptPRX(input, output, size);
    if (length <= 0)
        return -1;
    length = pspDecompress(output, input, capacity);
    if (length <= 0)
        return -1;
    for (int i = 0; i + 0x30 <= length; i++) {
        if (memcmp(input + i, "~PSP", 4) == 0) {
            u32 inner_size;
            memcpy(&inner_size, input + i + 0x2c, sizeof(inner_size));
            if (inner_size > (u32)(length - i))
                return -1;
            length = pspDecryptPRX(input + i, output, inner_size);
            if (length <= 0)
                return -1;
            if (decompress)
                length = pspDecompress(output, input, capacity);
            return length > 0 ? length : -1;
        }
    }
    return -1;
}

static int patchReboot(const char *path, const u8 *header, int region_size,
                       const u8 *reboot, int reboot_size, u8 *scratch, int capacity)
{
    int size = readFile(path, 0, scratch, capacity);
    if (size < region_size || reboot_size <= 0 ||
        reboot_size > region_size - REBOOT_HEADER_SIZE)
        return -1;
    for (int i = 0; i <= size - region_size; i++) {
        if (memcmp(scratch + i, header, REBOOT_HEADER_SIZE) == 0) {
            memcpy(scratch + i + REBOOT_HEADER_SIZE, reboot, reboot_size);
            SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_TRUNC, 0);
            if (fd < 0)
                return fd;
            int written = sceIoWrite(fd, scratch, size);
            int close_result = sceIoClose(fd);
            return written == size && close_result >= 0 ? 0 : -1;
        }
    }
    return -1;
}

static int prepareReboots(u8 *scratch, int scratch_size, u8 *out, u8 *out2, int out_size)
{
    static const u8 header150[REBOOT_HEADER_SIZE] = { REBOOT150_HEADER };
    static const u8 header661[REBOOT_HEADER_SIZE] = { REBOOT661_HEADER };
    const char *systemctrl = ARK_DC_PATH_150 "/kd/ark_systemctrl150.prx";
    int size = readFile(ARK_DC_PATH_150 "/kd/loadexec.prx", 0, out, out_size);
    if (size <= 0)
        return -1;
    int reboot_size = getReboot(out, out2, size, out_size, 0);
    if (reboot_size < 0 ||
        patchReboot(ARK_DC_PATH_150 "/reboot150.prx", header150, REBOOT150_COMP_SIZE,
                    out2, reboot_size, scratch, scratch_size) < 0 ||
        patchReboot(systemctrl, header150, REBOOT150_COMP_SIZE,
                    out2, reboot_size, scratch, scratch_size) < 0)
        return -1;

    size = readFile(ARK_DC_PATH "/kd/loadexec_01g.prx", 0, out, out_size);
    if (size < 0 && sceKernelDevkitVersion() == 0x06060110) {
        size = readFile("flash0:/kd/loadexec_01g.prx", 0, out, out_size);
        if (size == 0xBA00 && pspUnsignCheck(out) < 0)
            return -1;
    }
    if (size != 0xBA00)
        return -1;
    reboot_size = getReboot(out, out2, size, out_size, 1);
    if (reboot_size < 0)
        return -1;
    return patchReboot(systemctrl, header661, REBOOT661_COMP_SIZE,
                       out, reboot_size, scratch, scratch_size);
}

static int prepareMsIpl(u8 *scratch, u8 *out, u8 *out2, int out_size)
{
    SceUID fd = sceIoOpen("ms0:/150.PBP", PSP_O_RDONLY, 0);
    if (fd < 0)
        return fd;
    int result = -1;
    const int updater_size = 3737904;
    if (sceIoLseek(fd, 0x3251, PSP_SEEK_SET) != 0x3251 ||
        sceIoRead(fd, scratch, updater_size) != updater_size)
        goto done;
    int size = pspDecryptPRX(scratch, out2, updater_size);
    if (size < 0x303100 + 0x38480)
        goto done;
    size = pspDecryptPRX(out2 + 0x303100, out, 0x38480);
    if (size < 0x980 + 0x37000)
        goto done;
    size = pspDecryptIPL1(out + 0x980, out2, 0x37000);
    if (size <= 0)
        goto done;
    size = pspLinearizeIPL2(out2, out + 0x10000, size);
    if (size <= 0 || size > out_size - 0x10000)
        goto done;
    memset(out, 0, 0x10000);
    int prefix_size = readFile(ARK_DC_PATH_150 "/mainbinex.raw", 0, out, 0x10000);
    if (prefix_size <= 0)
        goto done;
    SceUID output = sceIoOpen(ARK_DC_PATH_150 "/msipl_01g.bin",
                              PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (output < 0)
        goto done;
    int written = sceIoWrite(output, out, size + 0x10000);
    int close_result = sceIoClose(output);
    result = written == size + 0x10000 && close_result >= 0 ? 0 : -1;
done:
    sceIoClose(fd);
    return result;
}

int Check150Files(u8 *scratch, int scratch_size)
{
    if (extractFiles("FLASH150.ARK", ARK_DC_PATH_150, ark150_files,
                     sizeof(ark150_files) / sizeof(ark150_files[0]), scratch, 16384, 0) < 0) {
        vlfGuiMessageDialog("FLASH150.ARK is missing or incomplete.", 1);
        return -1;
    }
    if (extractFiles("TM.PKG", "ms0:", tm150_files,
                     sizeof(tm150_files) / sizeof(tm150_files[0]), scratch, 16384, 0) < 0) {
        vlfGuiMessageDialog("TM.PKG is missing ARK-150 modules.", 1);
        return -1;
    }
    SceUID lang = openInput("LANG150.ARK");
    if (lang < 0) {
        vlfGuiMessageDialog("LANG150.ARK is missing.", 1);
        return lang;
    }
    SceOff size = sceIoLseek(lang, 0, PSP_SEEK_END);
    sceIoClose(lang);
    if (size <= 0 || size > 0x7fffffff) {
        vlfGuiMessageDialog("LANG150.ARK is empty or invalid.", 1);
        return -1;
    }
    int loadexec = readFile(ARK_DC_PATH "/kd/loadexec_01g.prx", 0, scratch, scratch_size);
    if (loadexec < 0 && sceKernelDevkitVersion() == 0x06060110)
        loadexec = readFile("flash0:/kd/loadexec_01g.prx", 0, scratch, scratch_size);
    if (loadexec != 0xBA00) {
        vlfGuiMessageDialog("A 6.61 loadexec_01g.prx is required.", 1);
        return -1;
    }
    return 0;
}

int Install150Files(u8 *scratch, int scratch_size, u8 *out, u8 *out2, int out_size)
{
    if (extractFiles("FLASH150.ARK", ARK_DC_PATH_150, ark150_files,
                     sizeof(ark150_files) / sizeof(ark150_files[0]), scratch, 16384, 1) < 0) {
        vlfGuiMessageDialog("Failed to extract ARK-150 modules.", 1);
        return -1;
    }
    SetInstallProgress(88);
    if (extractFiles("TM.PKG", "ms0:", tm150_files,
                     sizeof(tm150_files) / sizeof(tm150_files[0]), scratch, 16384, 1) < 0) {
        vlfGuiMessageDialog("Failed to extract ARK-150 modules.", 1);
        return -1;
    }
    SetInstallProgress(91);
    SceUID lang = openInput("LANG150.ARK");
    if (lang < 0) {
        vlfGuiMessageDialog("LANG150.ARK is missing.", 1);
        return lang;
    }
    SceOff size = sceIoLseek(lang, 0, PSP_SEEK_END);
    int result = -1;
    if (size > 0 && size <= 0x7fffffff && sceIoLseek(lang, 0, PSP_SEEK_SET) == 0)
        result = copyBytes(lang, ARK_DC_PATH_150 "/LANG150.ARK", size, scratch, 16384);
    sceIoClose(lang);
    if (result < 0) {
        vlfGuiMessageDialog("Failed to copy LANG150.ARK.", 1);
        return result;
    }
    SetInstallProgress(94);
    if (prepareReboots(scratch, scratch_size, out, out2, out_size) < 0) {
        vlfGuiMessageDialog("Failed to prepare 1.50 or 6.61 reboot payload.", 1);
        return -1;
    }
    SetInstallProgress(97);
    result = prepareMsIpl(scratch, out, out2, out_size);
    if (result < 0)
        vlfGuiMessageDialog("Failed to prepare the 1.50 Memory Stick IPL.", 1);
    return result;
}
