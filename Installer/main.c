#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <psppower.h>
#include <pspgum.h>
#include <kubridge.h>
#include <libpsardumper.h>
#include <pspdecrypt.h>
#include <vlf.h>

#include "menu_items.h"

PSP_MODULE_INFO("TimeMachine_Installer", 0x800, 1, 0);
PSP_HEAP_SIZE_KB(1024);
PSP_MAIN_THREAD_ATTR(0);

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define OUT_SIZE 2500000
#define TABLE_SIZE 0x4000
#define PATH_SIZE 512

char boot_path[256];
static int running = 1, selected_cfw, model;
static volatile int install_state, progress, phase;
static char error_msg[256];
static char destination[32];
static char pbp_path[PATH_SIZE], cfw_path[PATH_SIZE], tm_path[PATH_SIZE];
static u8 *psar, *out, *out2;
static int psar_capacity;
static char tables[12][TABLE_SIZE];
static int table_sizes[12];

void *malloc64(int size);

static int fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error_msg, sizeof(error_msg), fmt, ap);
    va_end(ap);
    return -1;
}

static int read_exact(SceUID fd, void *data, unsigned size)
{
    u8 *p = data;
    while (size) {
        int n = sceIoRead(fd, p, size);
        if (n <= 0 || (unsigned)n > size)
            return -1;
        p += n;
        size -= n;
    }
    return 0;
}

static int write_exact(SceUID fd, const void *data, unsigned size)
{
    const u8 *p = data;
    while (size) {
        int n = sceIoWrite(fd, p, size);
        if (n <= 0 || (unsigned)n > size)
            return -1;
        p += n;
        size -= n;
    }
    return 0;
}

static int valid_relative_path(const char *path)
{
    const char *part = path;
    if (!*path || *path == '/')
        return 0;
    while (*part) {
        const char *end = strchr(part, '/');
        size_t len = end ? (size_t)(end - part) : strlen(part);
        if (!len || (len == 1 && part[0] == '.') ||
            (len == 2 && part[0] == '.' && part[1] == '.') ||
            memchr(part, ':', len) || memchr(part, '\\', len))
            return 0;
        if (!end)
            return 1;
        part = end + 1;
        if (!*part)
            return 0;
    }
    return 0;
}

static int make_parent_dirs(const char *path)
{
    char tmp[PATH_SIZE];
    char *p;
    SceIoStat st;
    if (strlen(path) >= sizeof(tmp))
        return fail("Destination path is too long.");
    strcpy(tmp, path);
    p = strchr(tmp, ':');
    if (!p || p[1] != '/')
        return fail("Invalid destination path.");
    for (p += 2; *p; ++p) {
        if (*p != '/')
            continue;
        *p = 0;
        if (sceIoMkdir(tmp, 0777) < 0 &&
            (sceIoGetstat(tmp, &st) < 0 || !FIO_S_ISDIR(st.st_mode))) {
            *p = '/';
            return fail("Cannot create directory: %s", tmp);
        }
        *p = '/';
    }
    return 0;
}

static int output_path(char *path, size_t size, const char *relative)
{
    if (!valid_relative_path(relative) ||
        snprintf(path, size, "%s/%s", destination, relative) >= (int)size)
        return fail("Invalid package or firmware path: %s", relative);
    return 0;
}

static int write_data(const char *relative, const void *data, unsigned size)
{
    char path[PATH_SIZE];
    SceUID fd;
    int result;
    if (output_path(path, sizeof(path), relative) < 0 || make_parent_dirs(path) < 0)
        return -1;
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return fail("Cannot write %s (0x%08X).", path, fd);
    result = write_exact(fd, data, size);
    if (sceIoClose(fd) < 0 || result < 0)
        return fail("Error writing %s.", path);
    return 0;
}

static int find_input(const char *name, char *path, size_t size)
{
    const char *roots[] = { boot_path, "ms0:", "ef0:" };
    SceIoStat st;
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(roots); ++i) {
        if (snprintf(path, size, "%s/%s", roots[i], name) >= (int)size)
            continue;
        if (sceIoGetstat(path, &st) >= 0 && FIO_S_ISREG(st.st_mode))
            return 0;
    }
    return fail("Cannot find %s in installer folder or device root.", name);
}

static u32 little32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

/* DC, PRO and LME packages use the same count/size/name/data archive. */
static int extract_archive(const char *archive, int is_tm, int dry_run)
{
    static const char *pro_required[] = {
        "/kd/_systemctrl.prx", "/kd/_vshctrl.prx", "/kd/_stargate.prx",
        "/kd/_popcorn.prx", "/kd/_inferno.prx", "/kd/_galaxy.prx",
        "/kd/_march33.prx", "/vsh/module/_recovery.prx", "/vsh/module/_satelite.prx"
    };
    static const char *lme_required[] = {
        "/kd/vshctrl_02g.prx", "/kd/dax9660.prx", "/kd/inferno.prx",
        "/kd/isotope.prx", "/kd/pulsar.prx", "/kd/horoscope.prx",
        "/kd/idmanager.prx", "/vsh/module/recovery.prx", "/vsh/module/satellite.prx"
    };
    const char *const *required = selected_cfw ? lme_required : pro_required;
    unsigned required_count = selected_cfw ? ARRAY_SIZE(lme_required) : ARRAY_SIZE(pro_required);
    unsigned found = 0, count, i;
    char boot_name[40], system_name[40], prefix[32];
    u8 header[5], buffer[16384];
    SceUID fd = sceIoOpen(archive, PSP_O_RDONLY, 0);
    SceOff length;
    int result = -1;

    if (fd < 0)
        return fail("Cannot open %s (0x%08X).", archive, fd);
    length = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (length < 4 || sceIoLseek(fd, 0, PSP_SEEK_SET) < 0 ||
        read_exact(fd, header, 4) < 0 || !(count = little32(header)) || count > 256)
        goto invalid;
    snprintf(prefix, sizeof(prefix), "/TM/%s/", selected_cfw ? "661LME" : "661PRO");
    if (model == 0)
        strcpy(boot_name, "/kd/pspbtjnf.bin");
    else
        snprintf(boot_name, sizeof(boot_name), "/kd/pspbtjnf_%02dg.bin", model + 1);
    snprintf(system_name, sizeof(system_name), "/kd/systemctrl_%02dg.prx",
             model == 0 ? 1 : model == 1 ? 2 : model == 4 ? 5 : 3);

    for (i = 0; i < count; ++i) {
        char name[256], path[PATH_SIZE];
        const char *relative = NULL;
        u32 size, remaining;
        unsigned j;
        SceOff offset;
        SceUID dest;
        if (read_exact(fd, header, 5) < 0 || !header[4] ||
            !(size = little32(header)) || size > 8 * 1024 * 1024 ||
            read_exact(fd, name, header[4]) < 0)
            goto invalid;
        name[header[4]] = 0;
        if (memchr(name, 0, header[4]) || name[0] != '/' ||
            !valid_relative_path(name + 1))
            goto invalid;
        offset = sceIoLseek(fd, 0, PSP_SEEK_CUR);
        if (offset < 0 || offset > length || size > length - offset)
            goto invalid;

        if (is_tm) {
            if (!strcmp(name, "/TM/tmctrl.prx")) {
                relative = "tmctrl.prx";
                if (found & 1) goto invalid;
                found |= 1;
            } else if (!strncmp(name, prefix, strlen(prefix))) {
                relative = name + strlen(prefix);
                if (!strcmp(relative, "reboot.prx")) {
                    if (found & 2) goto invalid;
                    found |= 2;
                } else if (!strcmp(relative, "tmhlpr.prx")) {
                    if (found & 4) goto invalid;
                    found |= 4;
                }
            }
        } else {
            relative = name + 1;
            for (j = 0; j < required_count; ++j) {
                if (!strcmp(name, required[j])) {
                    if (found & (1u << j)) goto invalid;
                    found |= 1u << j;
                }
            }
            if (selected_cfw && !strcmp(name, boot_name)) {
                if (found & (1u << 9)) goto invalid;
                found |= 1u << 9;
            }
            if (selected_cfw && !strcmp(name, system_name)) {
                if (found & (1u << 10)) goto invalid;
                found |= 1u << 10;
            }
        }
        if (!relative || dry_run) {
            if (sceIoLseek(fd, size, PSP_SEEK_CUR) < 0) goto invalid;
            continue;
        }
        if (output_path(path, sizeof(path), relative) < 0 || make_parent_dirs(path) < 0)
            goto done;
        dest = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (dest < 0) {
            fail("Cannot write %s (0x%08X).", path, dest);
            goto done;
        }
        remaining = size;
        while (remaining) {
            unsigned n = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            if (read_exact(fd, buffer, n) < 0 || write_exact(dest, buffer, n) < 0) {
                sceIoClose(dest);
                fail("Error extracting %s.", path);
                goto done;
            }
            remaining -= n;
        }
        if (sceIoClose(dest) < 0) {
            fail("Error closing %s.", path);
            goto done;
        }
        scePowerTick(0);
    }
    if (sceIoLseek(fd, 0, PSP_SEEK_CUR) != length ||
        (is_tm ? found != 7 : found != ((1u << (required_count + (selected_cfw ? 2 : 0))) - 1)))
        goto invalid;
    result = 0;
    goto done;
invalid:
    fail("Invalid, incomplete or truncated package: %s", archive);
done:
    sceIoClose(fd);
    return result;
}

static int table_path(const char *table, int size, const char *number, char *path, size_t capacity)
{
    int i;
    for (i = 0; i + 6 < size; ++i) {
        int j = i + 6;
        size_t n = 0;
        if (memcmp(table + i, number, 5) || table[i + 5] != '|')
            continue;
        while (j < size && (unsigned char)table[j] >= 0x20) {
            if (n + 2 >= capacity)
                return -1;
            if (n == 6 && !memcmp(path, "flash", 5))
                path[n++] = ':';
            path[n++] = table[j++];
        }
        if (j == size)
            return -1;
        path[n] = 0;
        return 1;
    }
    return 0;
}

static int five_digits(const char *name)
{
    int i;
    if (strlen(name) != 5)
        return 0;
    for (i = 0; i < 5; ++i)
        if (name[i] < '0' || name[i] > '9') return 0;
    return 1;
}

static int extract_firmware(void)
{
    u8 header[0x28];
    SceUID fd = sceIoOpen(pbp_path, PSP_O_RDONLY, 0);
    SceOff length;
    u32 offset, expected_size = model == 4 ? 21569584 : 26848656;
    int buffered, base = 0, last_pos = 0, retry = 0, files_written = 0, result = -1;
    if (fd < 0)
        return fail("Cannot open %s (0x%08X).", pbp_path, fd);
    length = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (length < (SceOff)sizeof(header) || sceIoLseek(fd, 0, PSP_SEEK_SET) < 0 ||
        read_exact(fd, header, sizeof(header)) < 0 || memcmp(header, "\0PBP", 4))
        goto invalid;
    offset = little32(header + 0x24);
    if (offset != (model == 4 ? 0x6351a5u : 0x577635u) ||
        length - offset != expected_size || sceIoLseek(fd, offset, PSP_SEEK_SET) < 0)
        goto invalid;
    buffered = expected_size < (u32)psar_capacity ? expected_size : psar_capacity;
    if (read_exact(fd, psar, buffered) < 0)
        goto invalid;
    if (pspPSARInit(psar, out, out2) < 0) {
        fail("Cannot initialize firmware PSAR.");
        goto done;
    }
    memset(table_sizes, 0, sizeof(table_sizes));
    for (;;) {
        char name[128], path[256];
        int expanded = 0, pos = last_pos, signcheck = 0, res;
        res = pspPSARGetNextFile(psar, expected_size, out, out2, name, &expanded, &pos, &signcheck);
        if (res < 0) {
            int shift = pos - base, n;
            if (retry || shift <= 0 || shift > buffered || base + buffered >= (int)expected_size) {
                fail("PSAR decode error at 0x%08X.", pos);
                goto done;
            }
            memmove(psar, psar + shift, buffered - shift);
            buffered -= shift;
            base += shift;
            n = expected_size - base - buffered;
            if (n > psar_capacity - buffered) n = psar_capacity - buffered;
            if (read_exact(fd, psar + buffered, n) < 0) {
                fail("Error reading firmware PSAR.");
                goto done;
            }
            buffered += n;
            pspPSARSetBufferPosition(base);
            retry = 1;
            continue;
        }
        if (!res) break;
        retry = 0;
        last_pos = pos;
        if (expanded < 0 || expanded > OUT_SIZE) {
            fail("Invalid firmware file size.");
            goto done;
        }
        if (expanded && five_digits(name)) {
            int number = atoi(name), i, found = 0;
            if (number >= 1 && number <= 11) {
                int n = pspDecryptTable(out2, out, expanded, 4);
                if (n <= 0 || n > TABLE_SIZE) {
                    fail("Cannot decrypt %02dg firmware table.", number);
                    goto done;
                }
                memcpy(tables[number], out2, n);
                table_sizes[number] = n;
            } else if (number > 12) {
                for (i = 1; i <= 11 && !found; ++i) {
                    if (table_sizes[i])
                        found = table_path(tables[i], table_sizes[i], name, path, sizeof(path));
                    if (found < 0) {
                        fail("Invalid firmware file table.");
                        goto done;
                    }
                }
                if (found && !strncmp(path, "flash0:/", 8)) {
                    if (write_data(path + 8, out2, expanded) < 0)
                        goto done;
                    ++files_written;
                }
            }
        }
        if (pos >= 0 && pos <= (int)expected_size)
            progress = 5 + (int)((long long)pos * 80 / expected_size);
        scePowerTick(0);
    }
    if (!files_written) {
        fail("No firmware files found in PSAR.");
        goto done;
    }
    result = 0;
    goto done;
invalid:
    fail("Invalid %s firmware update.", model == 4 ? "661GO.PBP" : "661.PBP");
done:
    sceIoClose(fd);
    return result;
}

static int copy_registry(const char *name)
{
    char source[128], relative[128];
    SceUID fd;
    SceOff size;
    snprintf(source, sizeof(source), "flash1:/registry/%s", name);
    snprintf(relative, sizeof(relative), "registry/%s", name);
    fd = sceIoOpen(source, PSP_O_RDONLY, 0);
    if (fd < 0)
        return fail("Cannot open %s (0x%08X).", source, fd);
    size = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (size <= 0 || size > OUT_SIZE || sceIoLseek(fd, 0, PSP_SEEK_SET) < 0 ||
        read_exact(fd, out, size) < 0) {
        sceIoClose(fd);
        return fail("Cannot read %s.", source);
    }
    sceIoClose(fd);
    return write_data(relative, out, size);
}

static int load_module(const char *name)
{
    char path[PATH_SIZE];
    const char *roots[] = { boot_path, "ms0:/PSP/LIBS", "ef0:/PSP/LIBS" };
    int mod = -1;
    unsigned i;
    for (i = 0; i < ARRAY_SIZE(roots); ++i) {
        if (snprintf(path, sizeof(path), "%s/%s", roots[i], name) >= (int)sizeof(path))
            continue;
        mod = kuKernelLoadModule(path, 0, NULL);
        if (mod >= 0) {
            mod = sceKernelStartModule(mod, 0, NULL, NULL, NULL);
            if (mod < 0) return fail("Cannot start %s (0x%08X).", name, mod);
            return 0;
        }
    }
    return fail("Cannot load %s (0x%08X).", name, mod);
}

static int install_thread(SceSize args, void *argp)
{
    int locked = 0, success = 0;
    (void)args;
    (void)argp;
    if (scePowerLock(0) < 0) {
        fail("Cannot prevent system suspend.");
        goto done;
    }
    locked = 1;
    phase = 1;
    if (find_input(model == 4 ? "661GO.PBP" : "661.PBP", pbp_path, sizeof(pbp_path)) < 0 ||
        find_input(selected_cfw ? "FLASH661.LME" : "FLASH661.PRO", cfw_path, sizeof(cfw_path)) < 0 ||
        find_input("TM.PKG", tm_path, sizeof(tm_path)) < 0 ||
        extract_archive(cfw_path, 0, 1) < 0 || extract_archive(tm_path, 1, 1) < 0 ||
        load_module("libpsardumper.prx") < 0 || load_module("pspdecrypt.prx") < 0)
        goto done;
    out = malloc64(OUT_SIZE);
    out2 = malloc64(OUT_SIZE);
    psar_capacity = 28 * 1024 * 1024;
    psar = malloc64(psar_capacity);
    if (!psar) {
        psar_capacity = 8 * 1024 * 1024;
        psar = malloc64(psar_capacity);
    }
    if (!out || !out2 || !psar) {
        fail("Not enough memory to extract firmware.");
        goto done;
    }
    phase = 2;
    if (extract_firmware() < 0) goto done;
    phase = 3;
    progress = 86;
    if (extract_archive(cfw_path, 0, 0) < 0) goto done;
    phase = 4;
    progress = 94;
    if (extract_archive(tm_path, 1, 0) < 0 ||
        copy_registry("system.dreg") < 0 || copy_registry("system.ireg") < 0)
        goto done;
    progress = 100;
    success = 1;
done:
    free(psar);
    free(out);
    free(out2);
    if (locked) scePowerUnlock(0);
    install_state = success ? 2 : -1;
    return sceKernelExitDeleteThread(0);
}

static int on_cancel(int enter)
{
    if (!enter && !install_state) running = 0;
    return VLF_EV_RET_NOTHING;
}

static int install(int sel)
{
    SceUID thread;
    (void)sel;
    if (vlfGuiMessageDialog("Install 6.61 to Memory Stick?\nExisting files will be overwritten.",
            VLF_MD_TYPE_NORMAL | VLF_MD_BUTTONS_YESNO | VLF_MD_INITIAL_CURSOR_NO) != VLF_MD_YES)
        return VLF_EV_RET_NOTHING;
    snprintf(destination, sizeof(destination), "ms0:/TM/%s", selected_cfw ? "661LME" : "661PRO");
    vlfGuiCancelCentralMenu();
    vlfGuiCancelPreviousPageControl();
    vlfGuiCancelBottomDialog();
    install_state = 1;
    thread = sceKernelCreateThread("tm_install", install_thread, 0x18, 0x10000, 0, NULL);
    if (thread < 0) {
        fail("Cannot create installer thread (0x%08X).", thread);
        install_state = -1;
    } else {
        int res = sceKernelStartThread(thread, 0, NULL);
        if (res < 0) {
            sceKernelDeleteThread(thread);
            fail("Cannot start installer thread (0x%08X).", res);
            install_state = -1;
        }
    }
    return VLF_EV_RET_NOTHING;
}

static int main_menu(int page);

static int firmware_select(int page)
{
    if (page == 1) {
        selected_cfw = vlfGuiCentralMenuSelection();
        vlfGuiCancelNextPageControl();
        vlfGuiCancelCentralMenu();
        vlfGuiPreviousPageControl(main_menu);
        vlfGuiCentralMenu(ARRAY_SIZE(firmware), firmware, 0, install, 10, 10);
    }
    return VLF_EV_RET_NOTHING;
}

static int main_menu(int page)
{
    (void)page;
    vlfGuiCancelCentralMenu();
    vlfGuiCancelPreviousPageControl();
    vlfGuiCentralMenu(ARRAY_SIZE(cfws), cfws, selected_cfw, NULL, 10, 10);
    vlfGuiNextPageControl(firmware_select);
    return VLF_EV_RET_NOTHING;
}

int app_main(int args, char *argv[])
{
    static char *statuses[] = {
        "Preparing...", "Checking installation files...",
        "Extracting 6.61 firmware...", "Extracting CFW package...",
        "Installing Time Machine files..."
    };
    int title, status = -1, bar = -1, percent = -1, last_phase = -1, last_progress = -1;
    ScePspFMatrix4 matrix;
    ScePspFVector3 scale;
    (void)args;
    (void)argv;
    vlfGuiSystemSetup(1, 1, 1);
    title = vlfGuiAddText(0, 0, "Time Machine Installer");
    vlfGuiSetTitleBar(title, 0, 1, 0);
    vlfGuiSetBackgroundPlane(0x00000000);
    vlfGuiSetModelResource("system_plugin_bg", "mdl_bg");
    scale.x = scale.y = scale.z = 8.5f;
    gumLoadIdentity(&matrix);
    gumScale(&matrix, &scale);
    vlfGuiSetModelWorldMatrix(&matrix);
    model = kuKernelGetModel();
    if (model < 0 || model > 10 || model == 5 || model == 7 || model == 9) {
        vlfGuiMessageDialog("Unsupported PSP model.", VLF_MD_TYPE_ERROR | VLF_MD_BUTTONS_NONE);
        sceKernelDelayThread(3000000);
        sceKernelExitGame();
        return 0;
    }
    main_menu(0);
    vlfGuiBottomDialog(VLF_DI_CANCEL, VLF_DI_ENTER, 1, 0, VLF_DEFAULT, on_cancel);
    while (running) {
        if (install_state == 1) {
            if (status < 0) {
                status = vlfGuiAddText(80, 100, "");
                bar = vlfGuiAddProgressBar(136);
                percent = vlfGuiAddText(240, 148, "");
                vlfGuiSetTextAlignment(percent, VLF_ALIGNMENT_CENTER);
            }
            if (phase != last_phase) {
                last_phase = phase;
                vlfGuiSetText(status, statuses[phase]);
            }
            if (progress != last_progress) {
                char text[16];
                last_progress = progress;
                snprintf(text, sizeof(text), "%d%%", progress);
                vlfGuiProgressBarSetProgress(bar, progress);
                vlfGuiSetText(percent, text);
            }
        } else if (install_state == 2 || install_state == -1) {
            if (status >= 0) vlfGuiRemoveText(status);
            if (bar >= 0) vlfGuiRemoveProgressBar(bar);
            if (percent >= 0) vlfGuiRemoveText(percent);
            vlfGuiMessageDialog(install_state == 2 ? "Installation completed." : error_msg,
                (install_state == 2 ? VLF_MD_TYPE_NORMAL : VLF_MD_TYPE_ERROR) | VLF_MD_BUTTONS_NONE);
            sceKernelDelayThread(3000000);
            running = 0;
        }
        vlfGuiDrawFrame();
    }
    sceKernelExitGame();
    return 0;
}
