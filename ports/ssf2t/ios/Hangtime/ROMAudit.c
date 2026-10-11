#include "HangtimeCore.h"
#include "unzip.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
struct requirement { const char *name; unsigned size, crc; };
static const struct requirement required[] = {
    {"sfxe.03c", 524288, 0x2fa1f396u},
    {"sfxe.04a", 524288, 0xd0bc29c6u},
    {"sfxe.05", 524288, 0x65222964u},
    {"sfxe.06a", 524288, 0x8fe9f531u},
    {"sfxe.07", 524288, 0x8a7d0cb6u},
    {"sfxe.08", 524288, 0x74c24062u},
    {"sfx.09", 524288, 0x642fae3fu},
    {"sfx.13m", 2097152, 0xcf94d275u},
    {"sfx.15m", 2097152, 0x5eb703afu},
    {"sfx.17m", 2097152, 0xffa60e0fu},
    {"sfx.19m", 2097152, 0x34e825c5u},
    {"sfx.14m", 1048576, 0xb7cc32e7u},
    {"sfx.16m", 1048576, 0x8376ad18u},
    {"sfx.18m", 1048576, 0xf5b1b336u},
    {"sfx.20m", 1048576, 0x459d5c6bu},
    {"sfx.21m", 1048576, 0xe32854afu},
    {"sfx.23m", 1048576, 0x760f2927u},
    {"sfx.25m", 1048576, 0x1ee90208u},
    {"sfx.27m", 1048576, 0xf814400fu},
    {"sfx.01", 131072, 0xb47b8835u},
    {"sfx.02", 131072, 0x0022633fu},
    {"sfx.11m", 2097152, 0x9bdbd476u},
    {"sfx.12m", 2097152, 0xa05e3aabu},
    {"ssf2t.key", 20, 0x524d608eu},
};
bool ht_validate_rom(const char *path, char *report, size_t capacity) {
    if (!report || !capacity) return false;
    report[0] = 0;
    unzFile zip = unzOpen(path);
    if (!zip) { snprintf(report, capacity, "Invalid or unreadable ZIP archive."); return false; }
    bool found[sizeof(required)/sizeof(required[0])] = {false};
    int status = unzGoToFirstFile(zip);
    unsigned files = 0;
    while (status == UNZ_OK && ++files <= 1024) {
        unz_file_info info;
        if (unzGetCurrentFileInfo(zip, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK) break;
        for (unsigned i = 0; i < sizeof(required)/sizeof(required[0]); ++i) {
            if (info.crc != required[i].crc || info.uncompressed_size != required[i].size) continue;
            /* Read the payload too: central-directory checksums alone are untrusted. */
            if (unzOpenCurrentFile(zip) != UNZ_OK) continue;
            unsigned char buffer[16384]; unsigned total = 0; int n;
            while ((n = unzReadCurrentFile(zip, buffer, sizeof buffer)) > 0) { total += n; if (total > required[i].size) break; }
            int close_status = unzCloseCurrentFile(zip);
            if (n == 0 && total == required[i].size && close_status == UNZ_OK) found[i] = true;
        }
        status = unzGoToNextFile(zip);
    }
    unzClose(zip);
    unsigned missing = 0;
    for (unsigned i = 0; i < sizeof(required)/sizeof(required[0]); ++i) {
        if (!found[i]) {
            size_t used = strlen(report);
            if (used < capacity) snprintf(report + used, capacity - used, "%s%s (%08x)", missing ? "\n" : "Missing or invalid ROMs for SSF2T Euro 940223:\n", required[i].name, required[i].crc);
            missing++;
        }
    }
    return missing == 0;
}
