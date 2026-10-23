/* Reproduce the 640x480 anamorphic Dreamcast picture in both screen modes. */
#include "../ports/thps2/ios/Hangtime/HangtimeCore.c"
#include <assert.h>
#include <math.h>
static void expect(double value, double wanted) { assert(fabs(value-wanted)<0.000001); }
int main(void) {
    width=640; height=480; av.geometry.aspect_ratio=16.0f/9.0f;
    widescreen=true;
    expect(ht_display_aspect(false),16.0/9.0);
    expect(ht_display_aspect(true),16.0/9.0);
    /* Requested settings may change before the next core geometry update. */
    widescreen=false;
    expect(ht_display_aspect(true),16.0/9.0);
    av.geometry.aspect_ratio=4.0f/3.0f;
    expect(ht_display_aspect(true),4.0/3.0);
    av.geometry.aspect_ratio=1.25f;
    expect(ht_display_aspect(false),1.25);
    expect(ht_display_aspect(true),4.0/3.0);
    width=height=0;
    expect(ht_display_aspect(true),1.25);
    puts("PASS: anamorphic widescreen retains 16:9 in Arcade and Square, including option transitions");
}
