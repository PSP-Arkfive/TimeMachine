#ifndef TMVITA_FLASH_LOADER_H
#define TMVITA_FLASH_LOADER_H

#define TMVITA_PATH "ms0:/TM/tmvita.prx"
#define TMHLPR_FLASH_PATH "/kd/tmhlpr.prx"
#define TMVITA_FLASH_PATH "/kd/tmvita.prx"
#define FLASH_ENTRY_HEADER_SIZE 5
#define FLASH_ENTRY_OVERHEAD(path) (FLASH_ENTRY_HEADER_SIZE + sizeof(path) - 1)

static const char *flash_package_path;
static const char *helper_path;

static int readFlashFile(const char *path, void *buffer, u32 size)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
    if (fd < 0) return fd;
    int result = sceIoRead(fd, buffer, size);
    if (result >= 0 && result != (int)size) result = SCE_EIO;
    int close_result = sceIoClose(fd);
    if (result >= 0 && close_result < 0) result = close_result;
    return result < 0 ? result : 0;
}

static int readFlashEntry(u8 *entry, const char *name, const char *path, u32 size)
{
    u32 name_size = strlen(name);
    memcpy(entry, &size, sizeof(size));
    entry[4] = name_size;
    memcpy(entry + FLASH_ENTRY_HEADER_SIZE, name, name_size);

    return readFlashFile(path, entry + FLASH_ENTRY_HEADER_SIZE + name_size, size);
}

static int loadFlashWithHelper(void)
{
    u8 *flash = (u8 *)VITA_FLASH_EXTRA;
    SceIoStat archive_stat, helper_stat, control_stat;
    *(u32 *)flash = 0;

    int result = sceIoGetstat(flash_package_path, &archive_stat);
    if (result < 0) return result;
    result = sceIoGetstat(helper_path, &helper_stat);
    if (result < 0) return result;
    result = sceIoGetstat(TMVITA_PATH, &control_stat);
    if (result < 0) return result;

    SceOff available = MAX_FLASH0_SIZE - FLASH_ENTRY_OVERHEAD(TMHLPR_FLASH_PATH)
        - FLASH_ENTRY_OVERHEAD(TMVITA_FLASH_PATH);
    if (helper_stat.st_size <= 0 || helper_stat.st_size > available) return SCE_KERR_ILLEGAL_SIZE;
    available -= helper_stat.st_size;
    if (control_stat.st_size <= 0 || control_stat.st_size > available) return SCE_KERR_ILLEGAL_SIZE;
    available -= control_stat.st_size;
    if (archive_stat.st_size < 4 || archive_stat.st_size > available) return SCE_KERR_ILLEGAL_SIZE;

    u32 archive_size = archive_stat.st_size;
    u32 helper_size = helper_stat.st_size;
    u32 control_size = control_stat.st_size;
    u32 helper_entry_size = FLASH_ENTRY_OVERHEAD(TMHLPR_FLASH_PATH) + helper_size;
    u32 control_entry_size = FLASH_ENTRY_OVERHEAD(TMVITA_FLASH_PATH) + control_size;
    u8 *archive = flash + helper_entry_size + control_entry_size;

    result = readFlashFile(flash_package_path, archive, archive_size);
    if (result < 0) return result;

    u32 nfiles;
    memcpy(&nfiles, archive, sizeof(nfiles));
    if (nfiles == 0 || nfiles > (archive_size - 4) / (FLASH_ENTRY_HEADER_SIZE + 1))
        return SCE_EINVAL;

    u8 *entry = flash + 4;
    result = readFlashEntry(entry, TMHLPR_FLASH_PATH, helper_path, helper_size);
    if (result < 0) return result;
    result = readFlashEntry(entry + helper_entry_size, TMVITA_FLASH_PATH, TMVITA_PATH, control_size);
    if (result < 0) return result;

    *(u32 *)flash = nfiles + 2;
    return 0;
}

static u32 findJAL(u32 addr)
{
    if (!addr) return 0;
    for (;; addr += 4) {
        u32 instruction = _lw(addr);
        if (instruction >= 0x0C000000 && instruction < 0x10000000)
            return ((instruction & 0x03FFFFFF) << 2) | 0x80000000;
    }
}

static u64 kermit_flash_load(int cmd)
{
    u8 buf[128];
    u64 resp;
    void *alignedBuf = (void *)ALIGN_64((int)buf + 63);
    sceKernelDcacheInvalidateRange(alignedBuf, 0x40);
    SceKermitRequest *packet = (SceKermitRequest *)KERMIT_PACKET((int)alignedBuf);
    sceKermitSendRequest(packet, KERMIT_MODE_PERIPHERAL, cmd, 0, KERMIT_CALLBACK_DISABLE, &resp);
    return resp;
}

static int flashLoadPatch(int cmd)
{
    int ret = kermit_flash_load(cmd);
    if (cmd == KERMIT_CMD_ERROR_EXIT || cmd == KERMIT_CMD_ERROR_EXIT_2) {
        sceKernelDelayThread(10000);
        int load_result = loadFlashWithHelper();
        sceKernelDcacheWritebackAll();
        sceKernelIcacheInvalidateAll();
        if (load_result < 0) {
            return load_result;
        }
    }
    return ret;
}

static int patchKermitPeripheral(const char *package, const char *helper)
{
    u32 function = sctrlHENFindFunction("sceKermitPeripheral_Driver", "sceKermitPeripheral_driver", 0x0648E1A3);
    if (!function) return SCE_EINVAL;
    u32 swaddress = findJAL(function);
    if (!swaddress) return SCE_EINVAL;
    flash_package_path = package;
    helper_path = helper;
    REDIRECT_FUNCTION(swaddress, flashLoadPatch);
    return 0;
}

#endif
