#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <psptypes.h>
#include <pspgum.h>

#include <vlf.h>
#include <kubridge.h>
#include <systemctrl.h>
#include <systemctrl_se.h>
#include <systemctrl_ark.h>

#include "menu_items.h"

PSP_MODULE_INFO("TM_Installer", 0x800, 0, 0);
PSP_HEAP_SIZE_KB(1024);
PSP_MAIN_THREAD_ATTR(0);

char boot_path[256];

static volatile int g_running = 1;

int selected_firmware = 0;
const char *selected_usbdevice_path;
int is_psvita = 0;
static int psp_model;
static char *cfw_names[FIRMWARE_OPTION_COUNT];
static const FirmwareOption *available_firmwares[FIRMWARE_OPTION_COUNT];
static char *firmware_names[FIRMWARE_OPTION_COUNT];
static int cfw_count;
static int firmware_count;
static char *cefs[] = { "Install Time Machine for PS Vita" };

int OnCancelInstall(int enter) {

	if(!enter)
		g_running = 0;
	return 0;
}
extern int ExtractFirmware(void);
extern void FreeFirmwareBuffers(void);
extern void RemoveInstallProgress(void);
extern void InstallVita();
int main_menu(void);

static int OnPreviousPage(int page) {
	/* VLF's page-control cancellation must only run once per control. */
	vlfGuiCancelPreviousPageControl();
	return main_menu();
}

static int finish_text;
static char *finish_options[] = { "Back to main menu", "Exit" };

static int OnReturnToMainMenu(void *param) {
	vlfGuiBottomDialog(VLF_DI_CANCEL, -1, 1, 0, VLF_DEFAULT, OnCancelInstall);
	main_menu();
	return VLF_EV_RET_REMOVE_HANDLERS;
}

static int OnInstallChoice(int sel) {
	if (sel == 0) {
		if (vlfGuiAddEventHandler(0, -1, OnReturnToMainMenu, NULL) < 0) {
			vlfGuiMessageDialog("Failed to return to the main menu.", 1);
			return VLF_EV_RET_NOTHING;
		}
	} else {
		g_running = 0;
	}
	vlfGuiRemoveText(finish_text);
	return VLF_EV_RET_REMOVE_OBJECTS | VLF_EV_RET_REMOVE_HANDLERS;
}

static int OnInstallFinish(void *param) {
	int result = *(int *)param;
	RemoveInstallProgress();
	finish_text = vlfGuiAddText(20, 50, result == 0 ?
		"Installation complete." : "Installation failed.");
	vlfGuiCentralMenu(2, finish_options, 0, OnInstallChoice, 10, 10);
	return VLF_EV_RET_REMOVE_HANDLERS;
}

int install_thread(SceSize args, void *argp) {
	if (is_psvita) {
		InstallVita();
		g_running = 0;
	} else {
		static int result;
		result = ExtractFirmware();
		FreeFirmwareBuffers();
		if (result == 0)
			sceKernelDelayThread(500000);
		vlfGuiAddEventHandler(0, -1, OnInstallFinish, &result);
	}
	return sceKernelExitDeleteThread(0);

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
int LoadStartModule(char *module, int partition)
{
    SceUID mod = kuKernelLoadModule(module, 0, NULL);

    if (mod < 0)
        return mod;

    return sceKernelStartModule(mod, 0, NULL, NULL, NULL);
}


int start_firmware_install(int sel) {
	selected_firmware = available_firmwares[sel]->target;
	selected_usbdevice_path = available_firmwares[sel]->usbdevice_path;
	return ExtractFW(sel);
}

int firmware_select(int sel) {
	firmware_count = 0;
	for (unsigned int i = 0; i < FIRMWARE_OPTION_COUNT; i++) {
		if (psp_model <= firmware_options[i].max_model &&
		    strcmp(cfw_names[sel], firmware_options[i].cfw) == 0) {
			available_firmwares[firmware_count] = &firmware_options[i];
			firmware_names[firmware_count++] = firmware_options[i].name;
		}
	}
	vlfGuiCancelNextPageControl();
	vlfGuiCancelCentralMenu();
	vlfGuiPreviousPageControl(OnPreviousPage);
	vlfGuiCentralMenu(firmware_count, firmware_names, 0, start_firmware_install, 10, 10);

	return 0;

}

int main_menu() {
	vlfGuiCancelCentralMenu();
	if (is_psvita) {
		vlfGuiCentralMenu(1, cefs, 0, ExtractFW, 10, 10);
	} else {
		cfw_count = 0;
		for (unsigned int i = 0; i < FIRMWARE_OPTION_COUNT; i++) {
			if (psp_model > firmware_options[i].max_model)
				continue;
			int j;
			for (j = 0; j < cfw_count; j++)
				if (strcmp(cfw_names[j], firmware_options[i].cfw) == 0)
					break;
			if (j == cfw_count)
				cfw_names[cfw_count++] = firmware_options[i].cfw;
		}
		if (!cfw_count) {
			vlfGuiMessageDialog("No firmware supports this PSP model.", 1);
			g_running = 0;
			return 0;
		}
		vlfGuiCentralMenu(cfw_count, cfw_names, 0, firmware_select, 10, 10);
	}
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
		psp_model = kuKernelGetModel();
		if (psp_model < 0) {
			vlfGuiMessageDialog("Failed to determine PSP model.", 1);
			sceKernelExitGame();
			return 0;
		}
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

