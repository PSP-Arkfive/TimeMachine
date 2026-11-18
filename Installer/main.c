#include <pspsdk.h>
#include <pspkernel.h>
#include <kubridge.h>
#include <psptypes.h>
#include <pspgum.h>
#include <stddef.h>
#include <vlf.h>
#include <stdio.h>

#include "menu_items.h"

PSP_MODULE_INFO("TM_Installer", 0x800, 0, 0);
PSP_HEAP_SIZE_KB(1024);
PSP_MAIN_THREAD_ATTR(0);

char boot_path[256];

static int g_running = 1;

int selected_firmware = 0;

int OnCancelInstall(int enter) {

	if(!enter)
		g_running = 0;
	return 0;
}
extern void ExtractPSAR();

int install_thread(SceSize args, void *argp) {
	ExtractPSAR();
	g_running = 0;
	return 0;

}



int ExtractFW(int sel) { 
	int install_thid = sceKernelCreateThread("install_thread", install_thread, 0x18, 0x10000, 0, NULL);
    if (install_thid >= 0)
    {   
        sceKernelStartThread(install_thid, 0, 0); 
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
	vlfGuiCentralMenu(2, cfws, 0, firmware_select, 10, 10);
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


int app_main(int args, char *argv[]) {

	setup_ui();

	int mod = LoadStartModule("libpsardumper.prx", PSP_MEMORY_PARTITION_KERNEL);
	if(mod<0)
		vlfGuiMessageDialog("libpspsardumper.prx failed to load", 0);
	mod = LoadStartModule("pspdecrypt.prx", PSP_MEMORY_PARTITION_KERNEL);
	if(mod<0)
		vlfGuiMessageDialog("pspdecrypt.prx failed to load", 0);
	
	main_menu();
	
	while(g_running) { vlfGuiDrawFrame(); }
	sceKernelExitGame();
	return 0;
}

