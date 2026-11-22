#ifndef TM_UPDATER_H
#define TM_UPDATER_H

#include <stdint.h>
#include <string.h>

static uint32_t updaterWord(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int validateUpdaterSfo(const unsigned char *sfo, unsigned int size,
                              const char *version, int is_go)
{
    if (size < 20 || memcmp(sfo, "\0PSF", 4) != 0)
        return -1;
    uint32_t keys = updaterWord(sfo + 8);
    uint32_t values = updaterWord(sfo + 12);
    uint32_t count = updaterWord(sfo + 16);
    if (keys > size || values > size || count > (size - 20) / 16 ||
        20 + count * 16 > keys || keys > values)
        return -1;
    int found_version = 0, found_model = 0;
    for (uint32_t i = 0; i < count; i++) {
        const unsigned char *entry = sfo + 20 + i * 16;
        uint32_t key = entry[0] | (uint32_t)entry[1] << 8;
        uint32_t length = updaterWord(entry + 4);
        uint32_t value = updaterWord(entry + 12);
        if (key >= values - keys || !memchr(sfo + keys + key, 0, values - keys - key) ||
            value > size - values || length > size - values - value)
            return -1;
        const char *name = (const char *)sfo + keys + key;
        const unsigned char *data = sfo + values + value;
        if (strcmp(name, "UPDATER_VER") == 0) {
            if (found_version || length != strlen(version) + 1 ||
                memcmp(data, version, length) != 0)
                return -1;
            found_version = 1;
        } else if (strcmp(name, "BOOTABLE") == 0) {
            if (found_model || length != 4 || updaterWord(data) != (is_go ? 2U : 1U))
                return -1;
            found_model = 1;
        }
    }
    return found_version && found_model ? 0 : -1;
}

#endif
