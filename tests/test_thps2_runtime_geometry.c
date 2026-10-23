/* Real-ROM integration check: an AV query cannot discard the original FOV.
 * Supply the user's USA Dreamcast CHD and an existing scratch directory.
 * This links the same bridge and Flycast core used by both platform ports. */
#include "HangtimeCore.h"
#include "libretro.h"
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t word(unsigned offset) {
    uint32_t result = 0;
    const unsigned char *ram = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    if (!ram || retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) < offset + 4) return 0;
    memcpy(&result, ram + offset, 4);
    return result;
}
static int check(bool wide) {
    const uint32_t expected = wide ? 0x3fa66666 : 0x3f800000;
    const double aspect = wide ? 16.0 / 9.0 : 4.0 / 3.0;
    const double actual = ht_aspect_ratio();
    if (word(0x1d77a0) != expected || actual < aspect - .001 || actual > aspect + .001) {
        fprintf(stderr, "FAIL: wide=%d FOV=%08x expected=%08x aspect=%.9f expected=%.9f\n",
                wide, word(0x1d77a0), expected, actual, aspect);
        return 1;
    }
    return 0;
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    if (SDL_Init(SDL_INIT_VIDEO)) return 2;
    if (!ht_start(argv[1], argv[2])) { fprintf(stderr, "%s\n", ht_error()); return 2; }
    for (unsigned i = 0; i < 100; ++i) ht_run();
    int status = 0;
    if (check(true)) { status = 1; goto finish; }
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        struct retro_system_av_info info;
        for (unsigned query = 0; query < 5; ++query) retro_get_system_av_info(&info);
        ht_set_widescreen(false);
        /* Until the emulation applies the request, presentation stays 16:9. */
        if (ht_display_aspect(true) < 1.7) { status = 1; goto finish; }
        for (unsigned i = 0; i < 2; ++i) ht_run();
        if (check(false)) { status = 1; goto finish; }
        ht_set_widescreen(true);
        for (unsigned i = 0; i < 2; ++i) ht_run();
        if (check(true)) { status = 1; goto finish; }
    }
finish:
    ht_stop(); SDL_Quit();
    if (status) return status;
    puts("PASS: real THPS2 ROM retains matching FOV and geometry through repeated AV queries and three widescreen toggles.");
    return 0;
}
