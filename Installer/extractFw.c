#include <pspsdk.h>
#include <pspkernel.h>
#include <psppower.h>
#include <cfwmacros.h>
#include <string.h>
#include <libpsardumper.h>
#include <pspdecrypt.h>
#include <vlf.h>
#include <stdlib.h>
#include <stdio.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <kubridge.h>
#include <systemctrl.h>
#include "menu_items.h"
#include "updater.h"

static char flash_table[14][0x4000];
static int flash_table_size[14];
static u8 *g_dataPSAR;
static u8 *g_dataOut;
static u8 *g_dataOut2;
static int PSAR_BUFFER_SIZE;
void *malloc64(int size);
static const int SMALL_BUFFER_SIZE = 2500000;
static int et;
static int progress_bar, progress_text;
static volatile int progress;
static int target_reboot_extracted;

int StartInstallProgress(char *status)
{
    vlfGuiCancelCentralMenu();
    vlfGuiCancelPreviousPageControl();
    vlfGuiCancelBottomDialog();
    et = vlfGuiAddText(20, 50, status);
    progress = 0;
    progress_bar = vlfGuiAddProgressBar(136);
    progress_text = vlfGuiAddText(240, 148, "0%");
    vlfGuiSetTextAlignment(progress_text, VLF_ALIGNMENT_CENTER);
    return et;
}

static int UpdateProgress(void *param)
{
    static char text[8];
    int value = progress;
    snprintf(text, sizeof(text), "%d%%", value);
    vlfGuiProgressBarSetProgress(progress_bar, value);
    vlfGuiSetText(progress_text, text);
    return VLF_EV_RET_REMOVE_HANDLERS;
}

void SetInstallProgress(int value)
{
    if (value > 100)
        value = 100;
    if (value > progress) {
        progress = value;
        vlfGuiAddEventHandler(0, -2, UpdateProgress, NULL);
    }
}

void RemoveInstallProgress(void)
{
    while (vlfGuiRemoveEventHandler(UpdateProgress) == 0) {}
    vlfGuiRemoveProgressBar(progress_bar);
    vlfGuiRemoveText(progress_text);
    vlfGuiRemoveText(et);
}

void FreeFirmwareBuffers(void)
{
    free(g_dataOut);
    free(g_dataOut2);
    free(g_dataPSAR);
    g_dataOut = NULL;
    g_dataOut2 = NULL;
    g_dataPSAR = NULL;
}


#define PSAR_SIZE_150    10149440
#define ARK150_TARGET TARGET_150

int Install150Files(u8 *scratch, int scratch_size, u8 *out, u8 *out2, int out_size);
int Check150Files(u8 *scratch, int scratch_size);
int getReboot(u8 *input, u8 *output, int size, int capacity, int decompress);

int WriteFile(char *file, void *buf, int size);

extern int selected_firmware;
extern const char *selected_usbdevice_path;
extern char boot_path[];

char *TM_PATH[] = {
	[TARGET_661_PRO] = "ms0:/TM/661PRO",
	[TARGET_661_LME] = "ms0:/TM/661LME",
	[TARGET_DCARK] = "ms0:/TM/DCARK",
	[TARGET_150] = "ms0:/TM/DCARK/150",
	[TARGET_V661_ARK] = "ms0:/TM/V661ARK",
	[TARGET_V661_EPI] = "ms0:/TM/V661EPI",
	[TARGET_V661_TN] = "ms0:/TM/V661TN",
	[TARGET_620_PRO] = "ms0:/TM/620PRO",
	[TARGET_620_LME] = "ms0:/TM/620LME",
	[TARGET_639_PRO] = "ms0:/TM/639PRO",
	[TARGET_639_LME] = "ms0:/TM/639LME",
};

static const struct {
	const char *version;
	const char *updater;
	const char *go_updater;
	const char *archive;
	int needs_reboot;
} firmware_files[] = {
	[TARGET_661_PRO] = { "6.61", "661.PBP", "661GO.PBP", "FLASH661.PRO", 0 },
	[TARGET_661_LME] = { "6.61", "661.PBP", "661GO.PBP", "FLASH661.LME", 0 },
	[TARGET_620_PRO] = { "6.20", "620.PBP", "620GO.PBP", "FLASH620.PRO", 1 },
	[TARGET_620_LME] = { "6.20", "620.PBP", "620GO.PBP", "FLASH620.LME", 1 },
	[TARGET_639_PRO] = { "6.39", "639.PBP", "639GO.PBP", "FLASH639.PRO", 1 },
	[TARGET_639_LME] = { "6.39", "639.PBP", "639GO.PBP", "FLASH639.LME", 1 },
};


int is5Dnum(char *str) {
    int len = strlen(str);
    if (len != 5)
        return 0;

    int i;
    for (i = 0; i < len; i++) {
        if (str[i] < '0' || str[i] > '9')
            return 0;
    }

    return 1;
}


static int FindTablePath(char *table, int table_size, char *number, char *szOut) {
    int i, j, k;
    for (i = 0; i < table_size - 5; i++) {
        if (strncmp(number, table+i, 5) == 0) {
            for (j = 0, k = 0; i + j + 6 < table_size && k < 126; j++, k++) {
                if (table[i+j+6] < 0x20) {
                    szOut[k] = 0;
                    break;
                }

                if (table[i+5] == '|' && strncmp(table+i+6, "flash", 5) == 0 && j == 6) {
                    szOut[6] = ':';
                    szOut[7] = '/';
                    k++;
                } else if (table[i+5] == '|' && strncmp(table+i+6, "ipl", 3) == 0 && j == 3) {
                    szOut[3] = ':';
                    szOut[4] = '/';
                    k++;
                } else
                {
                    szOut[k] = table[i+j+6];
                }
            }
            return i + j + 6 < table_size && k < 126;
        }
    }

    return 0;
}



static int FindTablePath150(const char *table, int size, const char *number, char *out)
{
    for (int i = 0; i + 6 < size; i++) {
        if (strncmp(number, table + i, 5) != 0)
            continue;
        int j = 0, k = 0;
        while (i + 6 + j < size && (unsigned char)table[i + 6 + j] >= 0x20) {
            if (k >= 126)
                return 0;
            if ((j == 6 && !strncmp(table + i + 6, "flash", 5)) ||
                (j == 3 && !strncmp(table + i + 6, "ipl", 3))) {
                out[k++] = ':';
                out[k++] = '/';
            } else {
                out[k++] = table[i + 6 + j];
            }
            j++;
        }
        if (i + 6 + j >= size)
            return 0;
        out[k] = '\0';
        return 1;
    }
    return 0;
}

static int Extract150Entry(char *name, int size)
{
    int table = -1;
    if (strncmp(name, "com:", 4) == 0) table = 0;
    else if (strncmp(name, "01g:", 4) == 0) table = 1;
    else if (strncmp(name, "02g:", 4) == 0) table = 2;

    if (table >= 0) {
        if (strcmp(name + 4, "00000") == 0) {
            int table_size = pspDecryptTable(g_dataOut2, g_dataOut, size, 0);
            if (table_size <= 0 || table_size > sizeof(flash_table[table]))
                return -1;
            memcpy(flash_table[table], g_dataOut2, table_size);
            flash_table_size[table] = table_size;
            return 0;
        }
        if (!flash_table_size[table] ||
            !FindTablePath150(flash_table[table], flash_table_size[table], name + 4, name))
            return -1;
    }

    if (strncmp(name, "flash0:/", 8) != 0)
        return 0;
    if (strstr(name, ".."))
        return -1;

    char path[256];
    if (snprintf(path, sizeof(path), "%s/%s", TM_PATH[ARK150_TARGET], name + 8) >= sizeof(path))
        return -1;
    for (char *p = path + strlen(TM_PATH[ARK150_TARGET]) + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            SceIoStat stat;
            if (sceIoGetstat(path, &stat) < 0 && sceIoMkdir(path, 0777) < 0)
                return -1;
            *p = '/';
        }
    }
    return WriteFile(path, g_dataOut2, size) == size ? 0 : -1;
}

#define TM_PSP_FILE_COUNT 3

static int tmFileIndex(const char *name)
{
    if (strcmp(name, "/TM/tmctrl.prx") == 0)
        return 0;
    const char *dir = TM_PATH[selected_firmware] + 4;
    size_t len = strlen(dir);
    if (strncmp(name, dir, len) == 0) {
        if (strcmp(name + len, "/reboot.prx") == 0) return 1;
        if (strcmp(name + len, "/tmhlpr.prx") == 0) return 2;
    }
    return -1;
}

static int extractArchive(const char *archive, const char *dest_path,
                          int tm_package, int progress_start, int progress_end)
{
    unsigned char buf[16 * 1024];
    SceUID fdr = sceIoOpen(archive, PSP_O_RDONLY, 0);
    if (fdr < 0)
        return -1;
    int result = -1, filecount;
    unsigned int found = 0;
    SceOff total = sceIoLseek(fdr, 0, PSP_SEEK_END);
    if (total <= 0 || sceIoLseek(fdr, 0, PSP_SEEK_SET) != 0 ||
        sceIoRead(fdr, &filecount, sizeof(filecount)) != sizeof(filecount) || filecount < 0)
        goto done;
    for (int i = 0; i < filecount; i++) {
        int filesize;
        unsigned char namelen;
        char filename[256], filepath[384];
        if (sceIoRead(fdr, &filesize, sizeof(filesize)) != sizeof(filesize) || filesize < 0 ||
            sceIoRead(fdr, &namelen, sizeof(namelen)) != sizeof(namelen) || namelen == 0 ||
            sceIoRead(fdr, filename, namelen) != namelen || memchr(filename, '\0', namelen))
            goto done;
        filename[namelen] = '\0';
        SceOff position = sceIoLseek(fdr, 0, PSP_SEEK_CUR);
        if (position < 0 || position > total || filesize > total - position)
            goto done;
        int index = tm_package ? tmFileIndex(filename) : 0;
        if (index < 0) {
            if (sceIoLseek(fdr, filesize, PSP_SEEK_CUR) != position + filesize)
                goto done;
        } else {
            if ((tm_package && (filesize == 0 || (found & (1U << index)))) ||
                strstr(filename, "..") ||
                snprintf(filepath, sizeof(filepath), "%s/%s", dest_path,
                         filename[0] == '/' ? filename + 1 : filename) >= sizeof(filepath))
                goto done;
            SceUID fdw = sceIoOpen(filepath, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
            if (fdw < 0)
                goto done;
            int remaining = filesize;
            while (remaining > 0) {
                int amount = remaining < sizeof(buf) ? remaining : sizeof(buf);
                int read = sceIoRead(fdr, buf, amount);
                if (read <= 0 || sceIoWrite(fdw, buf, read) != read)
                    break;
                remaining -= read;
                scePowerTick(0);
            }
            int close_result = sceIoClose(fdw);
            if (remaining != 0 || close_result < 0)
                goto done;
            if (tm_package)
                found |= 1U << index;
        }
        position = sceIoLseek(fdr, 0, PSP_SEEK_CUR);
        if (position < 0)
            goto done;
        SetInstallProgress(progress_start + (int)((progress_end - progress_start) * position / total));
    }
    if (!tm_package || found == (1U << TM_PSP_FILE_COUNT) - 1)
        result = 0;
done:
    sceIoClose(fdr);
    return result;
}

static int InstallUsbDevice(void)
{
    char source[320], destination[320];
    if (snprintf(source, sizeof(source), "%s/usbdevice.prx", boot_path) >= (int)sizeof(source) ||
        snprintf(destination, sizeof(destination), "%s%s", TM_PATH[selected_firmware],
                 selected_usbdevice_path) >= (int)sizeof(destination))
        return -1;
    SceUID input = sceIoOpen(source, PSP_O_RDONLY, 0);
    if (input < 0)
        return -1;
    SceUID output = sceIoOpen(destination, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (output < 0) {
        sceIoClose(input);
        return -1;
    }

    int result = -1;
    int copied = 0;
    int read;
    while ((read = sceIoRead(input, g_dataOut, 16 * 1024)) > 0) {
        if (sceIoWrite(output, g_dataOut, read) != read)
            goto done;
        copied = 1;
        scePowerTick(0);
    }
    if (read == 0 && copied)
        result = 0;
done:
    if (sceIoClose(output) < 0)
        result = -1;
    if (sceIoClose(input) < 0)
        result = -1;
    if (result < 0)
        sceIoRemove(destination);
    return result;
}


static int ExtractPrxs(int cbFile, SceUID fd)
{
    int psar_pos = 0;

    if(pspPSARInit(g_dataPSAR, g_dataOut, g_dataOut2) < 0) {
		vlfGuiMessageDialog("PSAR Init Failed...", 1);
        return -1;
	}

    while (1)
    {
        char name[128];
        int cbExpanded;
        int pos;
        int signcheck;

        int res = pspPSARGetNextFile(g_dataPSAR, cbFile, g_dataOut, g_dataOut2, name, &cbExpanded, &pos, &signcheck);

        if (res < 0)
        {
            if (selected_firmware == ARK150_TARGET)
                return -1;
            int dpos = pos-psar_pos;
            if (dpos <= 0 || dpos > PSAR_BUFFER_SIZE)
                return -1;
            psar_pos = pos;

            memmove(g_dataPSAR, g_dataPSAR+dpos, PSAR_BUFFER_SIZE-dpos);

            if (sceIoRead(fd, g_dataPSAR+(PSAR_BUFFER_SIZE-dpos), dpos) <= 0)
                return -1;

            pspPSARSetBufferPosition(psar_pos);

            continue;

        }
        else if (res == 0) /* no more files */
        {
            break;
        }

        if (pos > 0) {
            int current = pos < cbFile ? pos : cbFile;
            SetInstallProgress(5 + (int)((u64)80 * current / cbFile));
        }

        if (cbExpanded > 0)
        {
            if (selected_firmware == ARK150_TARGET) {
                if (Extract150Entry(name, cbExpanded) < 0)
                    return -1;
                scePowerTick(0);
                continue;
            }
            char szDataPath[256];

            if (is5Dnum(name)) {
                int num = atoi(name);

                // Files from 01g-11g
                if (num >= 1 && num <= 11) {
                    flash_table_size[num] = pspDecryptTable(g_dataOut2, g_dataOut, cbExpanded, 4);
                    if (flash_table_size[num] <= 0 ||
                        flash_table_size[num] > sizeof(flash_table[num]))
                        return -1;
                    memcpy(flash_table[num], g_dataOut2, flash_table_size[num]);
                    continue;
                } else {
                    if (num > 12) {
                        int found = 0;

                        int i;
                        for (i = 1; i <= 11; i++) {
                            if (flash_table_size[i] > 0) {
                                found = FindTablePath(flash_table[i], flash_table_size[i], name, name);
                                if (found)
                                    break;
                            }
                        }

                        if (found) {
                            if (strncmp(name, "flash0", 6) == 0) {
								snprintf(szDataPath, sizeof(szDataPath), "%s/%s", TM_PATH[selected_firmware], name + 8);

                                res = WriteFile(szDataPath, g_dataOut2, cbExpanded);
                                if (res != cbExpanded)
                                    return -1;
								char *filename = strrchr(name, '/');
								if(filename) {
									filename++;
								    if(strncmp(filename, "loadexec_", 9) == 0 && strstr(filename, ".prx") != NULL ) {
                                        int old_firmware = firmware_files[selected_firmware].needs_reboot;
                                        char model_name[32];
                                        snprintf(model_name, sizeof(model_name), "loadexec_%02dg.prx",
                                                 kuKernelGetModel() + 1);
                                        if (old_firmware && strcmp(filename, model_name) != 0)
                                            continue;
                                        int decrypted_size = old_firmware ?
                                            getReboot(g_dataOut2, g_dataOut, cbExpanded, SMALL_BUFFER_SIZE, 1) :
                                            pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
										if (decrypted_size > 0) {
                                           char rebootpath[256];
                                           snprintf(rebootpath,sizeof(rebootpath),"%s/reboot_%s.bin",TM_PATH[selected_firmware],filename + 9);
                                           char *ext = strstr(rebootpath, ".prx");
                                           if (ext)
                                               strcpy(ext, ".bin");
                                           res = WriteFile(rebootpath, old_firmware ? g_dataOut2 : g_dataOut, decrypted_size);
                                           if (res != decrypted_size)
                                               return -1;
                                           if (old_firmware)
                                               target_reboot_extracted = 1;
                                       }
                                       else if (old_firmware)
                                           return -1;
                                   }
								}
                            }
                            else if (strstr(name, "ipl") == name)
                            {

                                int is1g = (strstr(name, "01g") != NULL);
                                int is2g = (strstr(name, "02g") != NULL);

                                if (is1g || is2g)
                                {
                                    if (is2g)
                                    {
                                        cbExpanded = pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
                                        if (cbExpanded <= 0 || cbExpanded > SMALL_BUFFER_SIZE)
                                            return -1;
                                        memcpy(g_dataOut2, g_dataOut, cbExpanded);
                                    }

                                    if (is2g)
                                    {
										char path[256];
										snprintf(path,  sizeof(path), "%s/ipl_02g.bin", TM_PATH[selected_firmware]);
                                        WriteFile(path, g_dataOut2, cbExpanded);
                                    }
                                    else
                                    {
										char path[256];
										snprintf(path, sizeof(path), "%s/ipl_01g.bin", TM_PATH[selected_firmware]);
                                        WriteFile(path, g_dataOut2, cbExpanded);
                                    }

                                    int cb1 = pspDecryptIPL1(g_dataOut2, g_dataOut, cbExpanded);
                                    if (cb1 <= 0 || cb1 > SMALL_BUFFER_SIZE)
                                        return -1;
                                    int cb2 = pspLinearizeIPL2(g_dataOut, g_dataOut2, cb1);
                                    if (cb2 <= 0 || cb2 > SMALL_BUFFER_SIZE)
                                        return -1;

                                    sceKernelDcacheWritebackAll();

                                    if (is2g)
                                    {
										char path[256];
										snprintf(path, sizeof(path), "%s/nandipl_02g.bin", TM_PATH[selected_firmware]);
                                        WriteFile(path, g_dataOut2, cb2);
                                    }
                                    else
                                    {
										char path[256];
										snprintf(path, sizeof(path), "%s/nandipl_01g.bin", TM_PATH[selected_firmware] );
                                        WriteFile(path, g_dataOut2, cb2);
                                    }
                                }
                                else
                                {
                                    cbExpanded = pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
                                    if (cbExpanded <= 0 || cbExpanded > SMALL_BUFFER_SIZE)
                                        return -1;
                                    memcpy(g_dataOut2, g_dataOut, cbExpanded);

                                    char iplpath[256];
                                    char* iplname = strstr(name, "ipl_");
                                    sprintf(iplpath, "%s/%s", TM_PATH[selected_firmware], iplname);

                                    char* ext = strstr(iplpath, ".ipl");
                                    if (ext) strcpy(ext, ".bin");

                                    WriteFile(iplpath, g_dataOut2, cbExpanded);
                                }
                            }
						} else {
                            continue;
                        }
                    } else {
                        continue;
                    }
                }
            }
        }

        scePowerTick(0);

    }

    return 0;
}


void createFolders(char *firmware) {


	static char *paths[] = {
		"/codepage",
		"/data",
		"/data/cert",
		"/dic",
		"/font",
		"/gps",
		"/kd",
		"/kd/resource",
		"/net",
		"/net/http",
		"/vsh",
		"/vsh/module",
		"/vsh/resource",
		"/vsh/etc",
		"/vsh/theme",
		"/vsh/theme",
		"/registry"
	};


	static char fullpath[128];

	sceIoMkdir("ms0:/TM", 0777);
	if (selected_firmware == ARK150_TARGET)
		sceIoMkdir(TM_PATH[TARGET_DCARK], 0777);
	sceIoMkdir(TM_PATH[selected_firmware], 0777);

	for ( int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
		snprintf(fullpath, sizeof(fullpath), "%s%s", TM_PATH[selected_firmware], paths[i]);
		sceIoMkdir(fullpath, 0777);
	}


}


int ExtractFirmware(void)
{
    StartInstallProgress("Extracting firmware please wait...");
	memset(flash_table, 0, sizeof(flash_table));
    memset(flash_table_size, 0, sizeof(flash_table_size));
    target_reboot_extracted = 0;

    int is150 = selected_firmware == ARK150_TARGET;
    int model = kuKernelGetModel();
    int out_size = is150 ? 4 * 1024 * 1024 : SMALL_BUFFER_SIZE;
    if (is150 && model != PSP_1000) {
        vlfGuiMessageDialog("ARK-150 requires a PSP-1000.", 1);
        return -1;
    }
    g_dataOut = malloc64(out_size);
    g_dataOut2 = malloc64(out_size);

    PSAR_BUFFER_SIZE = is150 ? PSAR_SIZE_150 : (8*1024*1024);
    g_dataPSAR = malloc64(PSAR_BUFFER_SIZE);
    if (!g_dataOut || !g_dataOut2 || !g_dataPSAR) {
        vlfGuiMessageDialog("Not enough memory to extract firmware.", 1);
        return -1;
    }

    if (is150) {
        static const u8 expected_md5[16] = {
            0xf5, 0x65, 0x54, 0xe2, 0xbe, 0x35, 0x64, 0x5a,
            0x76, 0x0b, 0x18, 0xed, 0x65, 0x05, 0xc4, 0xcc
        };
        u8 md5[16];
        SceUID fd = sceIoOpen("ms0:/150.PBP", PSP_O_RDONLY, 0);
        if (fd < 0) {
            vlfGuiMessageDialog("Place 150.PBP at the root of ms0:.", 1);
            return -1;
        }
        int valid = sceIoLseek32(fd, 0x393B81, PSP_SEEK_SET) == 0x393B81 &&
                    sceIoRead(fd, g_dataPSAR, PSAR_SIZE_150) == PSAR_SIZE_150;
        sceIoClose(fd);
        if (!valid || sceKernelUtilsMd5Digest(g_dataPSAR, PSAR_SIZE_150, md5) < 0 ||
            memcmp(md5, expected_md5, sizeof(md5)) != 0) {
            vlfGuiMessageDialog("Invalid or corrupted 150.PBP.", 1);
            return -1;
        }
        if (Check150Files(g_dataOut, out_size) < 0)
            return -1;
        createFolders(NULL);
        SetInstallProgress(5);
        if (ExtractPrxs(PSAR_SIZE_150, -1) < 0) {
            vlfGuiMessageDialog("Failed to extract 1.50 firmware.", 1);
            return -1;
        }
        SetInstallProgress(85);
        vlfGuiSetText(et, "Installing ARK-150...");
        if (Install150Files(g_dataPSAR, PSAR_BUFFER_SIZE, g_dataOut, g_dataOut2, out_size) < 0)
            return -1;
        SetInstallProgress(100);
        return 0;
    }

	
    const char *updater = model == PSP_GO ? firmware_files[selected_firmware].go_updater :
                                           firmware_files[selected_firmware].updater;
    char pbp_path[32], message[128];
    snprintf(pbp_path, sizeof(pbp_path), "ms0:/%s", updater);
    SceUID fd = sceIoOpen(pbp_path, PSP_O_RDONLY, 0);
    if (fd < 0 && model == PSP_GO) {
        snprintf(pbp_path, sizeof(pbp_path), "ms0:/%s", firmware_files[selected_firmware].updater);
        fd = sceIoOpen(pbp_path, PSP_O_RDONLY, 0);
    }
	if(fd < 0) {
        snprintf(message, sizeof(message), "Place %s at the root of ms0:.", updater);
		vlfGuiMessageDialog(message, 1);
        return -1;
	}

    unsigned char header[40];
    SceOff file_size = sceIoLseek(fd, 0, PSP_SEEK_END);
    int valid = file_size > 0 && file_size <= 0x7fffffff &&
        sceIoLseek(fd, 0, PSP_SEEK_SET) == 0 &&
        sceIoRead(fd, header, sizeof(header)) == sizeof(header) &&
        memcmp(header, "\0PBP", 4) == 0;
    u32 sfo_start = valid ? updaterWord(header + 8) : 0;
    u32 sfo_end = valid ? updaterWord(header + 12) : 0;
    u32 psar_start = valid ? updaterWord(header + 36) : 0;
    valid = valid && sfo_start >= sizeof(header) && sfo_end >= sfo_start &&
        sfo_end - sfo_start <= 65536 && psar_start >= sfo_end &&
        psar_start <= file_size && file_size - psar_start >= PSAR_BUFFER_SIZE &&
        sceIoLseek(fd, sfo_start, PSP_SEEK_SET) == sfo_start &&
        sceIoRead(fd, g_dataOut, sfo_end - sfo_start) == sfo_end - sfo_start &&
        validateUpdaterSfo(g_dataOut, sfo_end - sfo_start,
                           firmware_files[selected_firmware].version, model == PSP_GO) == 0;
    if (!valid || sceIoLseek(fd, psar_start, PSP_SEEK_SET) != psar_start ||
        sceIoRead(fd, g_dataPSAR, PSAR_BUFFER_SIZE) != PSAR_BUFFER_SIZE) {
        sceIoClose(fd);
        snprintf(message, sizeof(message), "Invalid or incorrect %s.", updater);
        vlfGuiMessageDialog(message, 1);
        return -1;
    }

    createFolders(NULL);
    sceKernelDelayThread(10000);
    SetInstallProgress(5);
    if (ExtractPrxs((int)(file_size - psar_start), fd) < 0) {
        sceIoClose(fd);
        snprintf(message, sizeof(message), "Failed to extract %s firmware.",
                 firmware_files[selected_firmware].version);
        vlfGuiMessageDialog(message, 1);
        return -1;
    }
    sceIoClose(fd);
    if (firmware_files[selected_firmware].needs_reboot) {
        char reboot[128];
        SceIoStat stat;
        snprintf(reboot, sizeof(reboot), "%s/reboot_%02dg.bin",
                 TM_PATH[selected_firmware], model + 1);
        if (!target_reboot_extracted || sceIoGetstat(reboot, &stat) < 0 ||
            stat.st_size <= 0 || stat.st_size > 0x20000) {
            vlfGuiMessageDialog("The updater is missing this model's reboot code.", 1);
            return -1;
        }
    }
    SetInstallProgress(85);
	static char extractText[32];
	snprintf(extractText, sizeof(extractText), "Extracting %s", firmware_files[selected_firmware].archive);
	vlfGuiSetText(et, extractText);
	sceIoChdir(boot_path);
    if (extractArchive(firmware_files[selected_firmware].archive, TM_PATH[selected_firmware], 0, 85, 93) < 0) {
        vlfGuiMessageDialog("Failed to extract the CFW package.", 1);
        return -1;
    }
    vlfGuiSetText(et, "Installing Time Machine modules...");
    if (extractArchive("TM.PKG", "ms0:", 1, 93, 99) < 0) {
        vlfGuiMessageDialog("TM.PKG is missing required modules or could not be extracted.", 1);
        return -1;
    }
    if (selected_usbdevice_path && InstallUsbDevice() < 0) {
        vlfGuiMessageDialog("Failed to install usbdevice.prx.", 1);
        return -1;
    }
    SetInstallProgress(100);
    return 0;
}

