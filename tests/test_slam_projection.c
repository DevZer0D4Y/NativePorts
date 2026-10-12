#include "gte.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int gpu_widescreen, gpu_widescreen_active;

static int project(int x, int wide) {
    psxCP2Regs regs;
    memset(&regs, 0, sizeof regs);
    regs.CP2C.r[0] = regs.CP2C.r[2] = regs.CP2C.r[4] = 4096;
    regs.CP2C.r[7] = 1000;
    regs.CP2C.r[24] = 160 << 16;
    regs.CP2C.r[25] = 120 << 16;
    regs.CP2C.r[26] = 256;
    regs.CP2D.p[0].sw.l = x;
    gpu_widescreen = wide;
    gpu_widescreen_active = 1;
    gteRTPS_sf1lm0(&regs, 0);
    assert(regs.CP2D.p[14].sw.h == 120);
    return regs.CP2D.p[14].sw.l;
}

int main(void) {
    for (int x = -900; x <= 900; x += 100) {
        int original = project(x, 0), wide = project(x, 1);
        assert(abs((wide - 160) * 4 - (original - 160) * 3) <= 4);
    }
    assert(project(700, 0) > 319);
    assert(project(700, 1) < 319);
    puts("PASS: real GTE projects extra scenery inside the guest clip window; Y unchanged.");
}
