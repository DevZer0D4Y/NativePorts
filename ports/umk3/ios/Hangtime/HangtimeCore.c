#include "HangtimeCore.h"
#include "libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
static SRWLOCK audio_mutex = SRWLOCK_INIT;
static void audio_lock(void) { AcquireSRWLockExclusive(&audio_mutex); }
static void audio_unlock(void) { ReleaseSRWLockExclusive(&audio_mutex); }
#else
#include <pthread.h>
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static void audio_lock(void) { pthread_mutex_lock(&audio_mutex); }
static void audio_unlock(void) { pthread_mutex_unlock(&audio_mutex); }
#endif
extern int sf_save_nvram(void);
extern void umk3_set_widescreen(int enable);

static bool loaded, initialized, ran_frame;
static char directory[4096], error_text[8192];
static uint16_t buttons[4];
static uint8_t pixels[2048 * 1024 * 4];
static unsigned width, height;
static enum retro_pixel_format pixel_format;
static struct retro_system_av_info av;
#define AUDIO_CAPACITY 32768
static int16_t audio_ring[AUDIO_CAPACITY * 2];
static unsigned audio_head, audio_count;
struct option { char key[128], value[128]; };
static struct option defaults[128];
static unsigned default_count;
static void log_message(enum retro_log_level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (level >= RETRO_LOG_WARN) vsnprintf(error_text, sizeof error_text, fmt, args);
    va_end(args);
}
static bool environment(unsigned command, void *data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = log_message; return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = directory; return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        pixel_format = *(enum retro_pixel_format *)data;
        return pixel_format <= RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned *)data = 0; return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        const struct retro_variable *v = data;
        default_count = 0;
        for (; v && v->key && default_count < 128; ++v) {
            const char *start = strchr(v->value, ';');
            if (!start) continue;
            while (*++start == ' ') {}
            struct option *o = &defaults[default_count++];
            snprintf(o->key, sizeof o->key, "%s", v->key);
            size_t n = strcspn(start, "|");
            snprintf(o->value, sizeof o->value, "%.*s", (int)n, start);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        if (!strcmp(v->key, "mame2003-plus_input_interface")) { v->value = "retropad"; return true; }
        if (!strcmp(v->key, "mame2003-plus_skip_disclaimer") || !strcmp(v->key, "mame2003-plus_skip_warnings")) { v->value = "enabled"; return true; }
        for (unsigned i = 0; i < default_count; i++)
            if (!strcmp(v->key, defaults[i].key)) { v->value = defaults[i].value; return true; }
        v->value = NULL; return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool *)data = false; return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE:
        snprintf(error_text, sizeof error_text, "%s", ((struct retro_message *)data)->msg); return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: return true;
    default: return false;
    }
}
static void video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data || w > 2048 || h > 1024) return;
    width = w; height = h;
    for (unsigned y = 0; y < h; ++y) {
        const uint8_t *row = (const uint8_t *)data + y * pitch;
        for (unsigned x = 0; x < w; ++x) {
            uint8_t *p = pixels + (y * w + x) * 4;
            if (pixel_format == RETRO_PIXEL_FORMAT_XRGB8888) {
                uint32_t value; memcpy(&value, row + x * 4, 4);
                p[0] = value >> 16; p[1] = value >> 8; p[2] = value;
            } else {
                uint16_t value; memcpy(&value, row + x * 2, 2);
                unsigned r = (value >> (pixel_format == RETRO_PIXEL_FORMAT_RGB565 ? 11 : 10)) & 31;
                unsigned g = (value >> 5) & (pixel_format == RETRO_PIXEL_FORMAT_RGB565 ? 63 : 31);
                unsigned b = value & 31;
                p[0] = r * 255 / 31; p[1] = g * 255 / (pixel_format == RETRO_PIXEL_FORMAT_RGB565 ? 63 : 31); p[2] = b * 255 / 31;
            }
            p[3] = 255;
        }
    }
}
static size_t audio_batch(const int16_t *samples, size_t frames) {
    audio_lock();
    /* Keep at most 100ms of audio to avoid accumulated output latency. */
    if (audio_count > 4800) { audio_head = 0; audio_count = 0; }
    for (size_t i = 0; i < frames && audio_count < AUDIO_CAPACITY; i++) {
        unsigned pos = (audio_head + audio_count++) % AUDIO_CAPACITY;
        audio_ring[pos * 2] = samples[i * 2]; audio_ring[pos * 2 + 1] = samples[i * 2 + 1];
    }
    audio_unlock();
    return frames;
}
static void audio_sample(int16_t l, int16_t r) { int16_t s[2] = {l,r}; audio_batch(s, 1); }
static void poll(void) {}
static int16_t input(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)index;
    return port < 4 && device == RETRO_DEVICE_JOYPAD && id < 16 ? !!(buttons[port] & (1u << id)) : 0;
}
bool ht_start(const char *path, const char *save_directory) {
    if (loaded) return false;
    error_text[0] = 0;
    if (!ht_validate_rom(path, error_text, sizeof error_text)) return false;
    snprintf(directory, sizeof directory, "%s", save_directory);
    ran_frame = false; width = height = 0; pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
    memset(buttons, 0, sizeof buttons); ht_audio_clear();
    retro_set_environment(environment);
    retro_set_video_refresh(video); retro_set_audio_sample(audio_sample);
    retro_set_audio_sample_batch(audio_batch); retro_set_input_poll(poll); retro_set_input_state(input);
    if (!initialized) { retro_init(); initialized = true; }
    struct retro_game_info game = {path, NULL, 0, NULL};
    loaded = retro_load_game(&game);
    if (loaded) { retro_get_system_av_info(&av); for (unsigned i = 0; i < 4; ++i) retro_set_controller_port_device(i, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)); }
    else if (!error_text[0]) snprintf(error_text, sizeof error_text, "Unable to start the arcade core.");
    return loaded;
}
void ht_run(void) { if (loaded) { retro_run(); ran_frame = true; } }
bool ht_save_data(void) { return loaded && ran_frame && sf_save_nvram(); }
void ht_stop(void) { if (loaded && ran_frame) ht_save_data(); if (loaded) retro_unload_game(); loaded = false; memset(buttons, 0, sizeof buttons); ht_audio_clear(); }
void ht_reset(void) { if (loaded) { if (ran_frame) ht_save_data(); retro_reset(); } }
void ht_set_buttons(unsigned p, uint16_t b) { if (p < 4) buttons[p] = b; }
const uint8_t *ht_pixels(void) { return pixels; }
unsigned ht_width(void) { return width; }
unsigned ht_height(void) { return height; }
double ht_fps(void) { return av.timing.fps > 0 ? av.timing.fps : 59.6374; }
double ht_sample_rate(void) { return av.timing.sample_rate > 0 ? av.timing.sample_rate : 48000; }
/* The arcade's 400 columns fill a 4:3 monitor; widescreen fights show 512. */
double ht_aspect_ratio(void) { return width > 400 ? 4.0 / 3.0 * width / 400.0 : 4.0 / 3.0; }
void ht_set_widescreen(bool enabled) { umk3_set_widescreen(enabled); }
const char *ht_error(void) { return error_text; }
void ht_audio_clear(void) { audio_lock(); audio_head = audio_count = 0; audio_unlock(); }
void ht_audio_read(float *left, float *right, unsigned frames) {
    audio_lock();
    for (unsigned i = 0; i < frames; i++) {
        left[i] = right[i] = 0;
        if (audio_count) {
            left[i] = audio_ring[audio_head * 2] / 32768.0f;
            right[i] = audio_ring[audio_head * 2 + 1] / 32768.0f;
            audio_head = (audio_head + 1) % AUDIO_CAPACITY; audio_count--;
        }
    }
    audio_unlock();
}
