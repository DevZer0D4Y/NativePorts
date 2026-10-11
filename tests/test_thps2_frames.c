/* Exercise the actual frontend bridge, with a small synthetic core frame. */
#include "../ports/thps2/ios/Hangtime/HangtimeCore.c"
#include <assert.h>
static unsigned reads, runs;
bool hw_prepare(struct retro_hw_render_callback *c) { return false; }
void hw_begin(void) {}
void hw_end(void) {}
void hw_read(uint8_t *out, unsigned w, unsigned h) { ++reads; memset(out, 0, w*h*4); }
void hw_release(void) {}
void retro_run(void) {
    int flags = 0;
    assert(environment(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &flags));
    assert(flags & RETRO_AV_ENABLE_AUDIO);
    assert(!!(flags & RETRO_AV_ENABLE_VIDEO) == present_frame);
    ++runs;
    /* A core may still call the video callback on skipped frames. */
    video(RETRO_HW_FRAME_BUFFER_VALID, 2, 2, 8);
    int16_t sample[] = {100, -100};
    audio_batch(sample, 1);
}
int main(void) {
    struct retro_variable timing = {"reicast_sh4_timing", NULL};
    assert(environment(RETRO_ENVIRONMENT_GET_VARIABLE, &timing));
    #ifdef _WIN32
    assert(!strcmp(timing.value, "accurate"));
    #else
    assert(!strcmp(timing.value, "legacy"));
    #endif
    loaded = true;
    ht_displayed_frame(1.0); ht_displayed_frame(1.016);
    assert(perf_presented == 2);
    for (unsigned step = 0; step < 4; ++step) ht_run_frame(step == 3);
    assert(runs == 4 && reads == 1 && audio_count == 4);
    assert(width == 2 && height == 2 && ran_frame && present_frame);
    ht_run();
    assert(runs == 5 && reads == 2 && audio_count == 5);
    loaded = false;
    ht_run_frame(false);
    assert(runs == 5 && present_frame);
    puts("PASS: 4 catch-up frames produce 1 readback, retain all audio, and restore normal presentation.");
}
