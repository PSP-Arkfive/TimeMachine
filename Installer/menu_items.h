#ifndef __MENU_UI__
#define __MENU_UI__

typedef struct {
	char *cfw;
	char *name;
	int target;
	int max_model;
	const char *usbdevice_path;
} FirmwareOption;

enum {
	TARGET_661_PRO, TARGET_661_LME, TARGET_DCARK, TARGET_150,
	TARGET_V661_ARK, TARGET_V661_EPI, TARGET_V661_TN,
	TARGET_620_PRO, TARGET_620_LME, TARGET_639_PRO, TARGET_639_LME
};

static const FirmwareOption firmware_options[] = {
	{ "PRO-D", "6.61", TARGET_661_PRO, PSP_STREET, "/kd/_usbdevice.prx" },
	{ "PRO-D", "6.39", TARGET_639_PRO, PSP_9000, "/kd/_usbdevice.prx" },
	{ "PRO-D", "6.20", TARGET_620_PRO, PSP_GO, "/kd/_usbdevice.prx" },
	{ "LME-3", "6.61", TARGET_661_LME, PSP_STREET, "/kd/usbdev.prx" },
	{ "LME-3", "6.39", TARGET_639_LME, PSP_9000, "/kd/usbdev.prx" },
	{ "LME-3", "6.20", TARGET_620_LME, PSP_GO, "/kd/usbdev.prx" },
	{ "ARK-5", "1.50", TARGET_150, PSP_1000, NULL }
};

#define FIRMWARE_OPTION_COUNT (sizeof(firmware_options) / sizeof(firmware_options[0]))

#endif
