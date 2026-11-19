#include <stddef.h>
#include <stdio.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <psptypes.h>
#include <pspgum.h>

#include <vlf.h>
#include <kubridge.h>
#include <systemctrl_se.h>
#include <systemctrl_ark.h>

#include "menu_items.h"

PSP_MODULE_INFO("TM_Installer", 0x800, 0, 0);
PSP_HEAP_SIZE_KB(1024);
PSP_MAIN_THREAD_ATTR(0);

char boot_path[256];

static int g_running = 1;

int selected_firmware = 0;
int is_psvita = 0;

int OnCancelInstall(int enter) {

	if(!enter)
		g_running = 0;
	return 0;
}
extern void Extract661PSAR();
extern void InstallVita();

int install_thread(SceSize args, void *argp) {
	if (is_psvita)
		InstallVita();
	else
		Extract661PSAR();
	g_running = 0;
	return 0;

}

int ExtractFW(int sel) { 
	int install_thid = sceKernelCreateThread("install_thread", install_thread, 0x18, 0x10000, 0, NULL);
	if (install_thid < 0) {
		vlfGuiMessageDialog("Failed to create installer thread.", 1);
		return VLF_EV_RET_NOTHING;
	}
	if (sceKernelStartThread(install_thid, 0, 0) < 0) {
		sceKernelDeleteThread(install_thid);
		vlfGuiMessageDialog("Failed to start installer thread.", 1);
		return VLF_EV_RET_NOTHING;
	}

    return VLF_EV_RET_REMOVE_HANDLERS;
}
int main_menu();


int LoadStartModule(char *module, int partition)
{
    SceUID mod = kuKernelLoadModule(module, 0, NULL);

    if (mod < 0)
        return mod;

    return sceKernelStartModule(mod, 0, NULL, NULL, NULL);
}


int firmware_select(int sel) {

	selected_firmware = sel;
	vlfGuiCancelNextPageControl();
	vlfGuiCancelCentralMenu();
	vlfGuiPreviousPageControl(main_menu);
	vlfGuiCentralMenu(1, firmware, 0, ExtractFW, 10, 10);

	return 0;

}

int main_menu() {
	vlfGuiCancelCentralMenu();
	vlfGuiCancelPreviousPageControl();
	vlfGuiCentralMenu(is_psvita ? 1 : 2, (is_psvita ? cefs : cfws), 0, (is_psvita ? ExtractFW : firmware_select), 10, 10);
	return 0;
}


void setup_ui() {
	int title = vlfGuiAddText(0, 0, "TM Installer");
	vlfGuiSetTitleBar(title, 0, 1, 0);
	vlfGuiSetBackgroundPlane(0x00000000);
	vlfGuiSystemSetup(1,1,1);
	vlfGuiSetModelResource("system_plugin_bg", "mdl_bg");
	ScePspFMatrix4 matrix;
    ScePspFVector3 scale;
    
    scale.x = scale.y = scale.z = 8.5f;
    gumLoadIdentity(&matrix);
    gumScale(&matrix, &scale);
    vlfGuiSetModelWorldMatrix(&matrix);

    vlfGuiBottomDialog(VLF_DI_CANCEL, -1, 1, 0, VLF_DEFAULT, OnCancelInstall);
}

int isVita(){
	SEConfig config;
	sctrlSEGetConfig(&config);

	if (config.adr.magic[0] == SECONFIG_MAGIC_EPI1 && config.adr.magic[1] == SECONFIG_MAGIC_EPI2)
		return 1;
	else if (config.ark.magic == ARK_CONFIG_MAGIC){
		ARKConfig ark_config;
		sctrlArkGetConfig(&ark_config);
		return IS_VITA_ADR(&ark_config);
	}
	return 0;
}

int app_main(int args, char *argv[]) {

	is_psvita = isVita();
	setup_ui();

	if (!is_psvita) {
		int mod = LoadStartModule("libpsardumper.prx", PSP_MEMORY_PARTITION_KERNEL);
		if(mod<0)
			vlfGuiMessageDialog("libpspsardumper.prx failed to load", 0);
		mod = LoadStartModule("pspdecrypt.prx", PSP_MEMORY_PARTITION_KERNEL);
		if(mod<0)
			vlfGuiMessageDialog("pspdecrypt.prx failed to load", 0);
	}
	
	main_menu();
	
	while(g_running) { vlfGuiDrawFrame(); }
	sceKernelExitGame();
	return 0;
}

