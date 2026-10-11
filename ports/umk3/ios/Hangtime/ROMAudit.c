#include "HangtimeCore.h"
#include "unzip.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
struct requirement { const char *name; unsigned size, crc; };
static const struct requirement required[] = {
 {"umk3-u2.bin", 1048576, 0x3838cfe5u},
 {"umk3-u3.bin", 1048576, 0x856fe411u},
 {"umk3-u4.bin", 1048576, 0x428a406fu},
 {"umk3-u5.bin", 1048576, 0x3b98a09fu},
 {"um312u54.bin", 524288, 0x712b4db6u},
 {"um312u63.bin", 524288, 0x6d301fafu},
 {"umk-u133.bin", 1048576, 0x79b94667u},
 {"umk-u132.bin", 1048576, 0x13e95228u},
 {"umk-u131.bin", 1048576, 0x41001e30u},
 {"umk-u130.bin", 1048576, 0x49379dd7u},
 {"umk-u129.bin", 1048576, 0xa8b41803u},
 {"umk-u128.bin", 1048576, 0xb410d72fu},
 {"umk-u127.bin", 1048576, 0xbd985be7u},
 {"umk-u126.bin", 1048576, 0xe7c32cf4u},
 {"umk-u125.bin", 1048576, 0x9a52227eu},
 {"umk-u124.bin", 1048576, 0x5c750ebcu},
 {"umk-u123.bin", 1048576, 0xf0ab88a8u},
 {"umk-u122.bin", 1048576, 0x9b87cdacu},
 {"umk-u121.bin", 1048576, 0xcc4b95dbu},
 {"umk-u120.bin", 1048576, 0x1c8144cdu},
 {"umk-u119.bin", 1048576, 0x5f10c543u},
 {"umk-u118.bin", 1048576, 0xde0c4488u},
 {"umk-u113.bin", 1048576, 0x99d74a1eu},
 {"umk-u112.bin", 1048576, 0xb5a46488u},
 {"umk-u111.bin", 1048576, 0xa87523c8u},
 {"umk-u110.bin", 1048576, 0x0038f205u},
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
            if (used < capacity) snprintf(report + used, capacity - used, "%s%s (%08x)", missing ? "\n" : "Missing or invalid ROMs for UMK3 rev 1.2:\n", required[i].name, required[i].crc);
            missing++;
        }
    }
    return missing == 0;
}
