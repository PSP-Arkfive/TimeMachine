#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../main.c"

SceUID sceIoOpen(const char *path, int flags, SceMode mode)
{
    int host_flags = flags & PSP_O_WRONLY ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY;
    return open(path, host_flags, mode);
}
int sceIoClose(SceUID fd) { return close(fd); }
int sceIoRead(SceUID fd, void *data, SceSize size) { return read(fd, data, size); }
int sceIoWrite(SceUID fd, const void *data, SceSize size) { return write(fd, data, size); }
SceOff sceIoLseek(SceUID fd, SceOff offset, int whence) { return lseek(fd, offset, whence); }
int sceIoMkdir(const char *path, SceMode mode) { return mkdir(path, mode); }
int sceIoGetstat(const char *path, SceIoStat *st)
{
    struct stat s;
    if (stat(path, &s) < 0) return -1;
    st->st_mode = S_ISDIR(s.st_mode) ? FIO_S_IFDIR : FIO_S_IFREG;
    return 0;
}
int scePowerTick(int type) { (void)type; return 0; }

static int psar_step, shifted;
int pspPSARInit(u8 *data, u8 *a, u8 *b)
{
    (void)data; (void)a; (void)b;
    psar_step = shifted = 0;
    return 0;
}
int pspPSARGetNextFile(u8 *data, int size, u8 *a, u8 *b, char *name,
                       int *expanded, int *pos, int *signcheck)
{
    static const char table[] = "12345|flash0/kd/example.prx\n";
    (void)data; (void)size; (void)a; (void)signcheck;
    switch (psar_step++) {
    case 0:
        strcpy(name, "00001");
        memcpy(b, table, sizeof(table) - 1);
        *expanded = sizeof(table) - 1;
        *pos = 100;
        return 1;
    case 1:
        return -1;
    case 2:
        assert(shifted == 100);
        strcpy(name, "12345");
        memcpy(b, "firmware", 8);
        *expanded = 8;
        *pos = 200;
        return 1;
    default:
        return 0;
    }
}
int pspPSARSetBufferPosition(int pos) { shifted = pos; return 0; }
int pspDecryptTable(u8 *a, u8 *b, int size, int mode)
{
    (void)a; (void)b; (void)mode;
    return size;
}

static void u32le(FILE *f, unsigned n)
{
    unsigned char b[] = { n, n >> 8, n >> 16, n >> 24 };
    assert(fwrite(b, 1, sizeof(b), f) == sizeof(b));
}

static void package(const char *path, const char *const *names, unsigned count)
{
    FILE *f = fopen(path, "wb");
    unsigned i;
    assert(f);
    u32le(f, count);
    for (i = 0; i < count; ++i) {
        size_t len = strlen(names[i]);
        u32le(f, len);
        fputc(len, f);
        assert(fwrite(names[i], 1, len, f) == len);
        assert(fwrite(names[i], 1, len, f) == len);
    }
    assert(fclose(f) == 0);
}

static void check_file(const char *path, const char *contents)
{
    char buf[256];
    FILE *f = fopen(path, "rb");
    size_t len = strlen(contents);
    assert(f && len < sizeof(buf));
    assert(fread(buf, 1, len, f) == len);
    assert(!memcmp(buf, contents, len));
    assert(fgetc(f) == EOF);
    fclose(f);
}

static void fake_pbp(const char *path, unsigned offset, unsigned size)
{
    FILE *f = fopen(path, "wb");
    unsigned char header[0x28] = { 0, 'P', 'B', 'P' };
    assert(f);
    header[0x24] = offset;
    header[0x25] = offset >> 8;
    header[0x26] = offset >> 16;
    header[0x27] = offset >> 24;
    assert(fwrite(header, 1, sizeof(header), f) == sizeof(header));
    assert(fseek(f, (long)offset + size - 1, SEEK_SET) == 0);
    fputc(0, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    static const char *tm[] = {
        "/TM/tmctrl.prx", "/TM/661LME/reboot.prx", "/TM/661LME/tmhlpr.prx",
        "/TM/661PRO/reboot.prx", "/TM/661PRO/tmhlpr.prx", "/TM/DCARK/reboot.prx"
    };
    static const char *lme[] = {
        "/kd/vshctrl_02g.prx", "/kd/dax9660.prx", "/kd/inferno.prx",
        "/kd/isotope.prx", "/kd/pulsar.prx", "/kd/horoscope.prx",
        "/kd/idmanager.prx", "/vsh/module/recovery.prx", "/vsh/module/satellite.prx",
        "/kd/pspbtjnf_05g.bin", "/kd/systemctrl_05g.prx"
    };
    static const char *pro[] = {
        "/kd/_systemctrl.prx", "/kd/_vshctrl.prx", "/kd/_stargate.prx",
        "/kd/_popcorn.prx", "/kd/_inferno.prx", "/kd/_galaxy.prx",
        "/kd/_march33.prx", "/vsh/module/_recovery.prx", "/vsh/module/_satelite.prx"
    };
    static const char *bad[] = {
        "/TM/tmctrl.prx", "/TM/661LME/reboot.prx", "/TM/661LME/../escape"
    };
    static const char *duplicate[] = {
        "/TM/tmctrl.prx", "/TM/tmctrl.prx",
        "/TM/661LME/reboot.prx", "/TM/661LME/tmhlpr.prx"
    };
    char path[128];
    assert(argc == 2 && chdir(argv[1]) == 0);
    assert(mkdir("ms0:", 0777) == 0);
    package("tm.pkg", tm, ARRAY_SIZE(tm));
    package("lme.pkg", lme, ARRAY_SIZE(lme));
    package("pro.pkg", pro, ARRAY_SIZE(pro));
    package("bad.pkg", bad, ARRAY_SIZE(bad));
    package("duplicate.pkg", duplicate, ARRAY_SIZE(duplicate));
    package("truncated.pkg", tm, ARRAY_SIZE(tm));
    {
        struct stat st;
        assert(stat("truncated.pkg", &st) == 0);
        assert(truncate("truncated.pkg", st.st_size - 1) == 0);
    }

    model = 4;
    selected_cfw = 1;
    strcpy(destination, "ms0:/TM/661LME");
    assert(extract_archive("tm.pkg", 1, 1) == 0);
    assert(extract_archive("lme.pkg", 0, 1) == 0);
    assert(extract_archive("bad.pkg", 1, 1) < 0);
    assert(extract_archive("duplicate.pkg", 1, 1) < 0);
    assert(extract_archive("truncated.pkg", 1, 1) < 0);
    assert(extract_archive("pro.pkg", 0, 1) < 0);
    assert(extract_archive("tm.pkg", 1, 0) == 0);
    assert(extract_archive("lme.pkg", 0, 0) == 0);
    check_file("ms0:/TM/661LME/tmctrl.prx", "/TM/tmctrl.prx");
    check_file("ms0:/TM/661LME/reboot.prx", "/TM/661LME/reboot.prx");
    check_file("ms0:/TM/661LME/tmhlpr.prx", "/TM/661LME/tmhlpr.prx");
    check_file("ms0:/TM/661LME/kd/systemctrl_05g.prx", "/kd/systemctrl_05g.prx");
    assert(access("ms0:/TM/661PRO/reboot.prx", F_OK) < 0);

    selected_cfw = 0;
    model = 0;
    strcpy(destination, "ms0:/TM/661PRO");
    assert(extract_archive("pro.pkg", 0, 1) == 0);
    assert(extract_archive("tm.pkg", 1, 0) == 0);
    assert(extract_archive("pro.pkg", 0, 0) == 0);
    check_file("ms0:/TM/661PRO/tmctrl.prx", "/TM/tmctrl.prx");
    check_file("ms0:/TM/661PRO/reboot.prx", "/TM/661PRO/reboot.prx");
    check_file("ms0:/TM/661PRO/kd/_systemctrl.prx", "/kd/_systemctrl.prx");

    assert(table_path("12345|flash0/kd/file.prx\n", 25, "12345", path, sizeof(path)) == 1);
    assert(!strcmp(path, "flash0:/kd/file.prx"));
    assert(table_path("12345|flash0/kd/file.prx", 24, "12345", path, sizeof(path)) < 0);
    psar_capacity = 512;
    psar = malloc(psar_capacity);
    out = malloc(OUT_SIZE);
    out2 = malloc(OUT_SIZE);
    assert(psar && out && out2);
    fake_pbp("661.PBP", 0x577635, 26848656);
    strcpy(pbp_path, "661.PBP");
    assert(extract_firmware() == 0);
    check_file("ms0:/TM/661PRO/kd/example.prx", "firmware");
    model = 4;
    fake_pbp("661GO.PBP", 0x6351a5, 21569584);
    strcpy(pbp_path, "661GO.PBP");
    assert(extract_firmware() == 0);
    model = 0;
    assert(extract_firmware() < 0);
    free(psar); free(out); free(out2);
    puts("installer tests passed");
    return 0;
}
