#ifndef TNV_FLASH_LOADER_H
#define TNV_FLASH_LOADER_H

#define FLASH0_TNV_PATH "ms0:/TM/V661TN/FLASH0.TN"
#define TMHLPR_PATH "ms0:/TM/V661TN/tmhlpr.prx"
#define TMHLPR_FLASH_PATH "/kd/tmhlpr.prx"
#define FLASH_ENTRY_HEADER_SIZE 5
#define TMHLPR_ENTRY_OVERHEAD (FLASH_ENTRY_HEADER_SIZE + (int)sizeof(TMHLPR_FLASH_PATH) - 1)

static u32 findJAL(u32 addr, int reversed, int skip)
{
    if (addr != 0) {
        int add = reversed ? -4 : 4;
        for (;; addr += add) {
            if ((_lw(addr) >= 0x0C000000) && (_lw(addr) < 0x10000000)) {
                if (skip == 0)
                    return (((_lw(addr) & 0x03FFFFFF) << 2) | 0x80000000);
                else
                    skip--;
            }
        }
    }

    return 0;
}

static int loadFlashWithHelper(void)
{
    u8 *flash = (u8 *)VITA_FLASH_EXTRA;
    SceIoStat stat;

    *(u32 *)flash = 0;
    int result = sceIoGetstat(TMHLPR_PATH, &stat);
    if (result < 0) return result;
    if (stat.st_size <= 0 || stat.st_size > MAX_FLASH0_SIZE - 4 - TMHLPR_ENTRY_OVERHEAD)
        return SCE_KERR_ILLEGAL_SIZE;
    u32 helper_size = stat.st_size;
    const u32 entry_size = TMHLPR_ENTRY_OVERHEAD + helper_size;
    // The helper replaces the original count after the archive is read.
    u8 *archive = flash + entry_size;

    SceUID fd = sceIoOpen(FLASH0_TNV_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return fd;

    SceOff archive_size = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (archive_size < 0) {
        result = archive_size;
    } else if (archive_size < 4 || archive_size > MAX_FLASH0_SIZE - entry_size) {
        result = SCE_KERR_ILLEGAL_SIZE;
    } else if ((result = sceIoLseek(fd, 0, PSP_SEEK_SET)) >= 0) {
        result = sceIoRead(fd, archive, (int)archive_size);
        if (result >= 0 && result != archive_size) result = SCE_EIO;
    }

    int close_result = sceIoClose(fd);
    if (result >= 0 && close_result < 0) result = close_result;
    if (result < 0) return result;

    u32 nfiles;
    memcpy(&nfiles, archive, sizeof(nfiles));
    if (nfiles == 0 || nfiles > ((u32)archive_size - 4) / (FLASH_ENTRY_HEADER_SIZE + 1))
        return SCE_EINVAL;

    u8 *entry = flash + 4;
    memcpy(entry, &helper_size, sizeof(helper_size));
    entry[4] = sizeof(TMHLPR_FLASH_PATH) - 1;
    memcpy(entry + FLASH_ENTRY_HEADER_SIZE, TMHLPR_FLASH_PATH, sizeof(TMHLPR_FLASH_PATH) - 1);

    fd = sceIoOpen(TMHLPR_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return fd;
    result = sceIoRead(fd, entry + TMHLPR_ENTRY_OVERHEAD, helper_size);
    if (result >= 0 && result != (int)helper_size) result = SCE_EIO;
    close_result = sceIoClose(fd);
    if (result >= 0 && close_result < 0) result = close_result;
    if (result < 0) return result;

    *(u32 *)flash = nfiles + 1;
    return 0;
}

// kermit_peripheral's sub_000007CC clone, called by loadexec + 0x0000299C with a0=8.
static u64 kermit_flash_load(int cmd)
{
    u8 buf[128];
    u64 resp;
    void *alignedBuf = (void*)ALIGN_64((int)buf + 63);
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
        if (load_result < 0) return load_result;
    }
    return ret;
}

static int patchKermitPeripheral(void)
{
    u32 function = sctrlHENFindFunction("sceKermitPeripheral_Driver", "sceKermitPeripheral_driver", 0x0648E1A3);
    if (!function) return SCE_EINVAL;
    u32 swaddress = findJAL(function, 0, 0);
    if (!swaddress) return SCE_EINVAL;
    REDIRECT_FUNCTION(swaddress, flashLoadPatch);
    return 0;
}

#endif
