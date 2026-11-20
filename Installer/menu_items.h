#ifndef __MENU_UI__
#define __MENU_UI__

typedef struct {
	char *cfw;
	char *name;
	int target;
	int max_model;
} FirmwareOption;

static const FirmwareOption firmware_options[] = {
	{ "PRO-D", "6.61", 0, PSP_STREET },
	{ "LME-3", "6.61", 1, PSP_STREET },
	{ "ARK-5", "1.50", 3, PSP_1000 }
};

#define FIRMWARE_OPTION_COUNT (sizeof(firmware_options) / sizeof(firmware_options[0]))

static char *cefs[] = {
	"Install Time Machine for PS Vita"
};

#endif
