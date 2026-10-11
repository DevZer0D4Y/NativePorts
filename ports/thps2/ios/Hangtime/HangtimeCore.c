#include "HangtimeCore.h"
#include "libretro.h"
#include "CoreGraphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
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


static bool loaded, initialized, ran_frame;
static bool present_frame = true;
static unsigned perf_frames, perf_reads, perf_presented;
static double perf_first_present, perf_last_present;
static double perf_core_ms, perf_read_ms, perf_started;
static double monotonic_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec time; clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000.0 + time.tv_nsec / 1000000.0;
#endif
}
static bool widescreen=true,options_dirty=false;
void ht_set_widescreen(bool enabled){if(widescreen!=enabled){widescreen=enabled;options_dirty=true;}}
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
    char message[8192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    if (level >= RETRO_LOG_WARN) snprintf(error_text, sizeof error_text, "%s", message);
    /* Keep engine boot diagnostics across a fatal core trap. The previous
       bridge kept only the last warning in RAM, which was lost on a crash. */
    if (level >= RETRO_LOG_INFO && directory[0]) {
        char path[8192];
        snprintf(path, sizeof path, "%s/thps2-boot.log", directory);
        FILE *file = fopen(path, "a");
        if (file) {
            if (fseek(file, 0, SEEK_END) == 0 && ftell(file) < 256 * 1024)
                fprintf(file, "[%d] %s\n", (int)level, message);
            fclose(file);
        }
    }
}
static bool environment(unsigned command, void *data) {
    switch (command) {
    case RETRO_ENVIRONMENT_SET_HW_RENDER:return hw_prepare(data);
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: *(unsigned *)data =
#ifdef _WIN32
 RETRO_HW_CONTEXT_OPENGL;
#else
 RETRO_HW_CONTEXT_OPENGLES2;
#endif
 return true;
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
        if(!strcmp(v->key,"reicast_widescreen_cheats")){v->value=widescreen?"enabled":"disabled";return true;}
        if(!strcmp(v->key,"reicast_widescreen_hack")){v->value="disabled";return true;}
        if(!strcmp(v->key,"reicast_sh4_timing")){
#ifdef _WIN32
            v->value="accurate";
#else
            v->value="legacy";
#endif
            return true;
        }
        if(!strcmp(v->key,"reicast_use_real_bios")){v->value="disabled";return true;}
        if (!strcmp(v->key, "mame2003-plus_input_interface")) { v->value = "retropad"; return true; }
        if (!strcmp(v->key, "mame2003-plus_skip_disclaimer") || !strcmp(v->key, "mame2003-plus_skip_warnings")) { v->value = "enabled"; return true; }
        for (unsigned i = 0; i < default_count; i++)
            if (!strcmp(v->key, defaults[i].key)) { v->value = defaults[i].value; return true; }
        v->value = NULL; return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool *)data = options_dirty;options_dirty=false;return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *(int *)data = RETRO_AV_ENABLE_AUDIO | (present_frame ? RETRO_AV_ENABLE_VIDEO : 0); return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:av.geometry=*(struct retro_game_geometry *)data;return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:av=*(struct retro_system_av_info *)data;return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE:
        snprintf(error_text, sizeof error_text, "%s", ((struct retro_message *)data)->msg); return true;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: return true;
    default: return false;
    }
}
static void video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!present_frame || !data || w > 1024 || h > 1024) return;
    if(data==RETRO_HW_FRAME_BUFFER_VALID){
        width=w;height=h;
        double started=monotonic_ms(); hw_read(pixels,w,h);
        perf_read_ms += monotonic_ms()-started; ++perf_reads; return;
    }
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
    char diagnostic_path[8192];
    snprintf(diagnostic_path, sizeof diagnostic_path, "%s/thps2-boot.log", directory);
    FILE *diagnostic = fopen(diagnostic_path, "w");
    if (diagnostic) {
        fprintf(diagnostic, "THPS2 native boot diagnostics, build 0.1.9\n");
        fclose(diagnostic);
    }
    ran_frame = false; width = height = 0; pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
    perf_frames=perf_reads=perf_presented=0; perf_first_present=perf_last_present=0; perf_core_ms=perf_read_ms=0; perf_started=monotonic_ms();
    memset(buttons, 0, sizeof buttons); ht_audio_clear();
    retro_set_environment(environment);
    retro_set_video_refresh(video); retro_set_audio_sample(audio_sample);
    retro_set_audio_sample_batch(audio_batch); retro_set_input_poll(poll); retro_set_input_state(input);
    if (!initialized) { retro_init(); initialized = true; }
    struct retro_game_info game = {path, NULL, 0, NULL};
    loaded = retro_load_game(&game);
    if (loaded) { retro_get_system_av_info(&av); for (unsigned i = 0; i < 4; ++i) retro_set_controller_port_device(i, RETRO_DEVICE_JOYPAD); }
    if(loaded) { size_t n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);void *p=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
      char path[8192];snprintf(path,sizeof path,"%s/save.srm",directory);FILE *f=fopen(path,"rb");if(f){if(p&&n)fread(p,1,n,f);fclose(f);} }
    else if (!error_text[0]) snprintf(error_text, sizeof error_text, "Unable to start the arcade core.");
    return loaded;
}
void ht_run_frame(bool present) {
    if (loaded) {
        double started=monotonic_ms();
        present_frame = present; hw_begin();retro_run();hw_end();ran_frame = true;
        perf_core_ms += monotonic_ms()-started;
        if (++perf_frames == 300) {
            double now=monotonic_ms(), elapsed=now-perf_started;
            log_message(RETRO_LOG_INFO, "PERF: %.1f emulated fps; core %.2f ms/frame (including readback); readback %.2f ms/read; %u reads/300 frames",
                elapsed>0 ? 300000.0/elapsed : 0, perf_core_ms/300,
                perf_reads ? perf_read_ms/perf_reads : 0, perf_reads);
            log_message(RETRO_LOG_INFO, "DISPLAY: %.1f presented fps; %u new video frames/300 emulated frames",
                perf_presented>1 && perf_last_present>perf_first_present ? (perf_presented-1)/(perf_last_present-perf_first_present) : 0, perf_reads);
            perf_frames=perf_reads=perf_presented=0; perf_first_present=perf_last_present=0; perf_core_ms=perf_read_ms=0; perf_started=now;
        }
    }
    present_frame = true;
}
void ht_run(void) { ht_run_frame(true); }
void ht_displayed_frame(double timestamp) {
    if (!loaded || timestamp <= 0) return;
    if (perf_presented == 0) perf_first_present = timestamp;
    perf_last_present = timestamp; ++perf_presented;
}
bool ht_save_data(void) {
    if(!loaded || !ran_frame)return false;
    const void *data=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if(!data || !n)return true;
    char path[8192];snprintf(path,sizeof path,"%s/save.srm",directory);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(data,1,n,f)==n;return fclose(f)==0 && ok;
}
void ht_stop(void) { if (loaded && ran_frame) ht_save_data(); if (loaded) {hw_release();retro_unload_game();} loaded = false; memset(buttons, 0, sizeof buttons); ht_audio_clear(); }
void ht_reset(void) { if (loaded) { if (ran_frame) ht_save_data(); retro_reset(); } }
void ht_set_buttons(unsigned p, uint16_t b) { if (p < 4) buttons[p] = b; }
const uint8_t *ht_pixels(void) { return pixels; }
unsigned ht_width(void) { return width; }
unsigned ht_height(void) { return height; }
double ht_fps(void) { return av.timing.fps > 0 ? av.timing.fps : 59.6374; }
double ht_sample_rate(void) { return av.timing.sample_rate > 0 ? av.timing.sample_rate : 48000; }
double ht_aspect_ratio(void) {return av.geometry.aspect_ratio>0?av.geometry.aspect_ratio:4.0/3.0;}
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
