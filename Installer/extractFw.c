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

static char flash_table[14][0x4000];
static int flash_table_size[14];
static u8 *g_dataPSAR;
static u8 *g_dataOut;
static u8 *g_dataOut2;
static int PSAR_BUFFER_SIZE;
void *malloc64(int size);
static const int SMALL_BUFFER_SIZE = 2500000;
static int et;


#define _661_UPDATER_VER_STR "6.61"
#define _639_UPDATER_VER_STR "6.39"
#define _661_UPDATER "661.PBP"
#define _639_UPDATER "639.PBP"
#define PSAR_SIZE_661    26848656
#define PSAR_SIZE_639    26096032

int WriteFile(char *file, void *buf, int size);

extern int selected_firmware;
extern char boot_path[];

char *TM_PATH[] = {
	"ms0:/TM/661PRO",
	"ms0:/TM/661LME",
	"ms0:/TM/639LME",
	"ms0:/TM/DCARK",
	"ms0:/TM/DCARK/150",
	"ms0:/TM/V661ARK",
	"ms0:/TM/V661EPI",
};

char *TM_FLASH[] = {
	"FLASH661.PRO",
	"FLASH661.LME",
	"FLASH639.LME"
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
            for (j = 0, k = 0; ; j++, k++) {
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

            return 1;
        }
    }

    return 0;
}



const char* findPath(const char** paths, int count){
    for (int i=0; i<count; i++){
        SceIoStat stat;
        int found = sceIoGetstat(paths[i], &stat);
        if (found >= 0){
            return paths[i];
        }
    }
    return NULL;
}


static const char* pbp_paths[] = {
    "ms0:/661.PBP",
    "ms0:/639.PBP",
};

void extractArchive(const char* archive, const char* dest_path)
{
    int buf_size = 16 * 1024;
    unsigned char buf[buf_size];
    int path_len = strlen(dest_path)+1;
    static char filepath[128];
    static char filename[128];
    strncpy(filepath, dest_path, path_len);
    strcat(filepath, "/");
    
    int fdr = sceIoOpen(archive, PSP_O_RDONLY, 0777);
	vlfGuiSetText(et, (char*)archive);
    
    if (fdr>=0){
        int filecount;
        sceIoRead(fdr, &filecount, sizeof(filecount));
        for (int i=0; i<filecount; i++){
            filepath[path_len] = '\0';
            int filesize;
            sceIoRead(fdr, &filesize, sizeof(filesize));

            unsigned char namelen;
            sceIoRead(fdr, &namelen, sizeof(namelen));

            sceIoRead(fdr, filename, namelen);
            filename[namelen] = '\0';
    
            strcat(filepath, (filename[0]=='/')?filename+1:filename);
            int fdw = sceIoOpen(filepath, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
            if (fdw < 0){ 
               sceIoClose(fdr);
            }   
            while (filesize>0){
                int read = sceIoRead(fdr, buf, (filesize>buf_size)?buf_size:filesize);
                sceIoWrite(fdw, buf, read);
                filesize -= read;
            }   
            sceIoClose(fdw);
        }   
        sceIoClose(fdr);
    }   
}


static void ExtractPrxs(int cbFile, SceUID fd)
{
    int psar_pos = 0;

    if(pspPSARInit(g_dataPSAR, g_dataOut, g_dataOut2) < 0) {
		vlfGuiMessageDialog("PSAR Init Failed...", 1);
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
            int dpos = pos-psar_pos;
            psar_pos = pos;

            memmove(g_dataPSAR, g_dataPSAR+dpos, PSAR_BUFFER_SIZE-dpos);

            sceIoRead(fd, g_dataPSAR+(PSAR_BUFFER_SIZE-dpos), dpos);

            pspPSARSetBufferPosition(psar_pos);

            continue;

        }
        else if (res == 0) /* no more files */
        {
            break;
        }

        if (cbExpanded > 0)
        {
            char szDataPath[256];

            if (is5Dnum(name)) {
                int num = atoi(name);

                // Files from 01g-11g
                if (num >= 1 && num <= 11) {
                    flash_table_size[num] = pspDecryptTable(g_dataOut2, g_dataOut, cbExpanded, 4);
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
                                        memcpy(g_dataOut2, g_dataOut, cbExpanded);
                                    }

                                    if (is2g)
                                    {
										char path[24];
										snprintf(path,  sizeof(path), "%s/ipl_02g.bin", TM_PATH[selected_firmware]);
                                        WriteFile(path, g_dataOut2, cbExpanded);
                                    }
                                    else
                                    {
										char path[24];
										snprintf(path, sizeof(path), "%s/ipl_01g.bin", TM_PATH[selected_firmware]);
                                        WriteFile(path, g_dataOut2, cbExpanded);
                                    }

                                    int cb1 = pspDecryptIPL1(g_dataOut2, g_dataOut, cbExpanded);
                                    int cb2 = pspLinearizeIPL2(g_dataOut, g_dataOut2, cb1);

                                    sceKernelDcacheWritebackAll();

                                    if (is2g)
                                    {
										char path[64];
										snprintf(path, sizeof(path), "%s/nandipl_02g.bin", TM_PATH[selected_firmware]);
                                        WriteFile(path, g_dataOut2, cb2);
                                    }
                                    else
                                    {
										char path[64];
										snprintf(path, sizeof(path), "%s/nandipl_01g.bin", TM_PATH[selected_firmware] );
                                        WriteFile(path, g_dataOut2, cb2);
                                    }
                                }
                                else
                                {
                                    cbExpanded = pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
                                    memcpy(g_dataOut2, g_dataOut, cbExpanded);

                                    char iplpath[256];
                                    char* iplname = strstr(name, "ipl_");
                                    sprintf(iplpath, "%s/%s", TM_PATH[selected_firmware], iplname);

                                    char* ext = strstr(iplpath, ".ipl");
                                    if (ext) strcpy(ext, ".bin");

                                    WriteFile(iplpath, g_dataOut2, cbExpanded);
                                }
                            }
							else if (strstr(name, "loadexec") != NULL)
							{    
								
								cbExpanded = pspDecryptPRX(g_dataOut2, g_dataOut, cbExpanded);
                                memcpy(g_dataOut2, g_dataOut, cbExpanded);

                                char rebootpath[256];
                                char* rebootname = strstr(name, "reboot_");
                                sprintf(rebootpath, "%s/%s", TM_PATH[selected_firmware], rebootname);

                                char* ext = strstr(rebootpath, ".prx");
                                if (ext) strcpy(ext, ".bin");

								// Now search for the embedded ~PSP (reboot)
                                u32 *p = (u32 *)g_dataOut2;
                                //int found = 0;
                                int i;
                            
                                for (i = 0; i < (cbExpanded / 4) - 0x10; i++)
                                {
                                    if (p[i] == 0x5053507E)   // "~PSP"
                                    {
                                        u32 rebootSize = p[i + 0x2C/4];   // size at +0x2C
                            
                                        if (rebootSize > 0x1000 && rebootSize < 0x20000 &&
                                            (i*4 + rebootSize) <= cbExpanded)
                                        {
                                            // Copy the embedded block
                                            memcpy(g_dataOut, g_dataOut2 + i*4, rebootSize);
                            
                                            // Decrypt + decompress the embedded reboot
                                            int rebootDec = pspDecryptPRX(g_dataOut, g_dataOut2, rebootSize);
                                            if (rebootDec > 0)
                                            {
                                                char outpath[256];
                                                int is02g = (strstr(name, "02g") != NULL);
                            
                                                if (is02g)
                                                    snprintf(outpath, sizeof(outpath), "%s/reboot_02g.bin", TM_PATH[selected_firmware]);
                                                else
                                                    snprintf(outpath, sizeof(outpath), "%s/reboot.bin", TM_PATH[selected_firmware]);
                            
                                                WriteFile(outpath, g_dataOut2, rebootDec);
                                                found = 1;
                                            }
                                            break;
                                        }
                                    }
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
	for(int i = 0; i < sizeof(TM_PATH)/sizeof(TM_PATH[0]); i++) {
		sceIoMkdir(TM_PATH[i], 0777);
	}
	//sceIoMkdir(TM_PATH[selected_firmware], 0777);

	for ( int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
		snprintf(fullpath, sizeof(fullpath), "%s%s", TM_PATH[selected_firmware], paths[i]);
		sceIoMkdir(fullpath, 0777);
	}


}


void ExtractPSAR()
{
	vlfGuiCancelCentralMenu();
	vlfGuiCancelPreviousPageControl();
	vlfGuiCancelBottomDialog();
	et = vlfGuiAddText(20, 50, "Extracting firmware please wait...");
	memset(flash_table, 0, sizeof(flash_table));
    memset(flash_table_size, 0, sizeof(flash_table_size));

    g_dataOut = malloc64(SMALL_BUFFER_SIZE);
    g_dataOut2 = malloc64(SMALL_BUFFER_SIZE);


	PSAR_BUFFER_SIZE = (8*1024*1024);
    g_dataPSAR = malloc64(PSAR_BUFFER_SIZE);

	
	static char firmware[4] = {0};
	if(selected_firmware == 0)
		snprintf(firmware, sizeof(firmware), "%s", "PRO");
	else
		snprintf(firmware, sizeof(firmware), "%s", "LME");

	createFolders(firmware);

	sceKernelDelayThread(800000);

    SceUID fd;

    const char* pbp_path = NULL;
    pbp_path = findPath(pbp_paths, NELEMS(pbp_paths));

    fd = sceIoOpen(pbp_path, PSP_O_RDONLY, 0);
	if(fd < 0) {
		vlfGuiMessageDialog((char*)pbp_path, 1);
	}

    sceIoLseek32(fd, 0x577635, PSP_SEEK_SET);
    sceIoRead(fd, g_dataPSAR, PSAR_BUFFER_SIZE);

    sceKernelDelayThread(10000);

	if(strncmp(TM_PATH[selected_firmware], "6.39", 4) == 0)
    	ExtractPrxs(PSAR_SIZE_639, fd);
	else
    	ExtractPrxs(PSAR_SIZE_661, fd);
    sceIoClose(fd);
	char extractText[32];
	snprintf(extractText, sizeof(extractText), "Extracting %s", TM_FLASH[selected_firmware]);
	vlfGuiSetText(et, extractText);
	sceIoChdir(boot_path);
	extractArchive(TM_FLASH[selected_firmware], TM_PATH[selected_firmware]);
	extractArchive("TM.PKG", "ms0:");
}

