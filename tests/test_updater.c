#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Installer/updater.h"

static void checkUpdater(const char *path, const char *version, int is_go)
{
    unsigned char header[40], sfo[65536];
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fread(header, 1, sizeof(header), file) == sizeof(header));
    assert(memcmp(header, "\0PBP", 4) == 0);
    unsigned int start = updaterWord(header + 8);
    unsigned int end = updaterWord(header + 12);
    unsigned int psar = updaterWord(header + 36);
    assert(end >= start && end - start <= sizeof(sfo) && psar >= end);
    assert(fseek(file, start, SEEK_SET) == 0);
    assert(fread(sfo, 1, end - start, file) == end - start);
    assert(validateUpdaterSfo(sfo, end - start, version, is_go) == 0);
    assert(validateUpdaterSfo(sfo, end - start, version, !is_go) < 0);
    assert(validateUpdaterSfo(sfo, end - start, "0.00", is_go) < 0);
    assert(validateUpdaterSfo(sfo, 19, version, is_go) < 0);
    sfo[16] = 0xff;
    sfo[17] = 0xff;
    sfo[18] = 0xff;
    sfo[19] = 0xff;
    assert(validateUpdaterSfo(sfo, end - start, version, is_go) < 0);
    assert(fseek(file, psar, SEEK_SET) == 0);
    assert(fread(header, 1, 4, file) == 4);
    assert(memcmp(header, "PSAR", 4) == 0);
    fclose(file);
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s 620.PBP 620GO.PBP 639.PBP 639GO.PBP\n", argv[0]);
        return 2;
    }
    checkUpdater(argv[1], "6.20", 0);
    checkUpdater(argv[2], "6.20", 1);
    checkUpdater(argv[3], "6.39", 0);
    checkUpdater(argv[4], "6.39", 1);
    puts("Updater metadata checks passed.");
    return 0;
}
