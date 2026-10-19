#include "HangtimeCore.h"
#include "libretro.h"
#include "AudioFIFO.h"
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
static char directory[4096], error_text[8192];
static uint16_t buttons[4];
static uint8_t pixels[2048 * 1024 * 4];
static unsigned width, height;
static uint64_t video_serial;
static enum retro_pixel_format pixel_format;
static struct retro_system_av_info av;
static HTAudioFIFO audio_fifo;
struct core_option_default { char key[128], value[128]; };
static struct core_option_default defaults[128];
static unsigned default_count;
static FILE *boot_log;
static uint64_t bench_ns;
static uint32_t bench_audio_hash = 2166136261u;
static unsigned bench_audio_samples;
static unsigned bench_frames;
static bool benchmark;
static unsigned capture_frame, capture_state_frame;
static bool load_benchmark_state;
/* Frame-timing diagnostics: where does each emulated frame's time go? */
static uint64_t ht_now_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    uint64_t ticks = (uint64_t)counter.QuadPart, hz = (uint64_t)frequency.QuadPart;
    return (ticks / hz) * 1000000000ull + (ticks % hz) * 1000000000ull / hz;
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}
static uint64_t perf_run_ns, perf_run_n, perf_vid_ns, perf_vid_n, perf_last_ns;
static void log_message(enum retro_log_level level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (boot_log) { va_list copy; va_copy(copy,args); vfprintf(boot_log,fmt,copy); va_end(copy); fflush(boot_log); }
    if (level >= RETRO_LOG_WARN) vsnprintf(error_text, sizeof error_text, fmt, args);
    va_end(args);
}
static bool environment(unsigned command, void *data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = log_message; return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av.geometry = *(struct retro_game_geometry *)data; return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        av = *(struct retro_system_av_info *)data; return true;
    case RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY:
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
            struct core_option_default *o = &defaults[default_count++];
            snprintf(o->key, sizeof o->key, "%s", v->key);
            size_t n = strcspn(start, "|");
            snprintf(o->value, sizeof o->value, "%.*s", (int)n, start);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        if (!strcmp(v->key,"mame_buttons_profiles")) { v->value="disabled";return true;}
        /* Enable MAME's multi-threaded work queue: the Voodoo poly rasterizer
           (poly.h) then spreads triangle fill across cores instead of running
           on one. Big win for this 3D arcade core; the flag only gates the work
           queue / osd_get_num_processors, not any whole-emulation threading. */
        if (!strcmp(v->key,"mame_thread_mode")) { v->value="enabled";return true;}
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
    uint64_t _vt0 = ht_now_ns();
    width = w; height = h; video_serial++;
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
    perf_vid_ns += ht_now_ns() - _vt0; perf_vid_n++;
}
static size_t audio_batch(const int16_t *samples, size_t frames) {
    if (benchmark) {
        const uint8_t *bytes = (const uint8_t *)samples;
        for (size_t i = 0; i < frames * 4; ++i) bench_audio_hash = (bench_audio_hash ^ bytes[i]) * 16777619u;
        bench_audio_samples += (unsigned)frames;
    }
    audio_lock();
    ht_fifo_push(&audio_fifo, samples, frames);
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
    char disk[8192];
    snprintf(disk,sizeof disk,"%s/nbashowt/nbashowt.chd",save_directory);
    if (!ht_validate_disk(disk,error_text,sizeof error_text)) {
        snprintf(error_text,sizeof error_text,"Import nbashowt.chd into the nbashowt folder beside nbashowt.zip. Both arcade files are required.");
        return false;
    }
    snprintf(directory, sizeof directory, "%s", save_directory);
    char log_path[8192]; snprintf(log_path,sizeof log_path,"%s/nbashowt-boot.log",directory);
    if (boot_log) fclose(boot_log);
    boot_log=fopen(log_path,"w");
    bench_audio_hash = 2166136261u; bench_audio_samples = 0;
    bench_ns = 0; bench_frames = 0; benchmark = getenv("SHOWTIME_BENCHMARK") != NULL;
    capture_frame = getenv("SHOWTIME_CAPTURE_FRAME") ? (unsigned)atoi(getenv("SHOWTIME_CAPTURE_FRAME")) : 0;
    capture_state_frame = getenv("SHOWTIME_SAVE_STATE_FRAME") ? (unsigned)atoi(getenv("SHOWTIME_SAVE_STATE_FRAME")) : 0;
    load_benchmark_state = getenv("SHOWTIME_LOAD_STATE") != NULL;
    perf_run_ns = perf_run_n = perf_vid_ns = perf_vid_n = perf_last_ns = 0;
    ran_frame = false; width = height = 0; video_serial = 0; pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
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
    else if (!error_text[0]) snprintf(error_text, sizeof error_text, "Unable to start the Showtime arcade core.");
    return loaded;
}
void ht_run(void) {
    if (!loaded) return;
    uint64_t _rt0 = ht_now_ns();
    retro_run();
    ran_frame = true;
    if (benchmark && load_benchmark_state) {
        load_benchmark_state = false;
        char path[8192]; snprintf(path, sizeof path, "%s/showtime-benchmark.state", directory);
        FILE *f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
            void *state = n > 0 && n <= 256 * 1024 * 1024 ? malloc((size_t)n) : NULL;
            bool ok = state && fread(state, 1, (size_t)n, f) == (size_t)n && retro_unserialize(state, (size_t)n);
            free(state); fclose(f); ht_audio_clear();
            if (boot_log) { fprintf(boot_log, "BENCH state restore=%d\n", ok); fflush(boot_log); }
        }
    }
    if (benchmark) {
        bench_ns += ht_now_ns() - _rt0;
        if (++bench_frames % 120 == 0 && boot_log) {
            uint32_t hash = 2166136261u;
            for (unsigned i = 0; i < width * height * 4; ++i) hash = (hash ^ pixels[i]) * 16777619u;
            fprintf(boot_log, "BENCH frame=%u run_ms=%.3f image=%08x audio=%08x samples=%u\n", bench_frames, bench_ns / 120e6, hash, bench_audio_hash, bench_audio_samples);
            fflush(boot_log);
            if (capture_frame == bench_frames && width && height) {
                char capture_path[8192]; snprintf(capture_path, sizeof capture_path, "%s/showtime-benchmark.ppm", directory);
                FILE *capture = fopen(capture_path, "wb");
                if (capture) {
                    fprintf(capture, "P6\n%u %u\n255\n", width, height);
                    for (unsigned i = 0; i < width * height; ++i) fwrite(pixels + i * 4, 1, 3, capture);
                    fclose(capture);
                }
            }
            bench_ns = 0; bench_audio_hash = 2166136261u; bench_audio_samples = 0;
        }
        if (capture_state_frame == bench_frames && capture_state_frame) {
            size_t n = retro_serialize_size();
            void *state = n && n <= 256 * 1024 * 1024 ? malloc(n) : NULL;
            bool ok = state && retro_serialize(state, n);
            if (ok) {
                char path[8192]; snprintf(path, sizeof path, "%s/showtime-benchmark.state", directory);
                FILE *f = fopen(path, "wb");
                if (f) { ok = fwrite(state, 1, n, f) == n; if (fclose(f)) ok = false; } else ok = false;
            }
            free(state);
            if (boot_log) { fprintf(boot_log, "BENCH state save=%d bytes=%zu frame=%u\n", ok, n, bench_frames); fflush(boot_log); }
        }
    }
    perf_run_ns += ht_now_ns() - _rt0; perf_run_n++;
    uint64_t _t = ht_now_ns();
    if (perf_last_ns == 0) perf_last_ns = _t;
    if (_t - perf_last_ns >= 2000000000ull && boot_log) {
        double rms = perf_run_n ? (double)perf_run_ns / perf_run_n / 1e6 : 0;
        double vms = perf_vid_n ? (double)perf_vid_ns / perf_vid_n / 1e6 : 0;
        audio_lock();
        unsigned underruns = audio_fifo.underruns, queued = audio_fifo.count, dropped = audio_fifo.dropped;
        audio_unlock();
        double elapsed = (double)(_t - perf_last_ns) / 1e9;
        double speed = perf_run_n / (ht_fps() * elapsed);
        fprintf(boot_log, "PERF run=%.2fms speed=%.1f%% video=%.2fms res=%ux%u fmt=%d audio_underruns=%u queued=%u dropped=%u\n",
                rms, speed * 100, vms, width, height, (int)pixel_format, underruns, queued, dropped);
        fflush(boot_log);
        perf_run_ns = perf_run_n = perf_vid_ns = perf_vid_n = 0; perf_last_ns = _t;
    }
}
/* Arcade projection changes require game-specific work; preserve native geometry. */
void ht_set_widescreen(bool enabled) { (void)enabled; }
bool ht_save_data(void) {
    if(!loaded || !ran_frame)return false;
    const void *data=retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    size_t n=retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if(!data || !n)return true;
    char path[8192];snprintf(path,sizeof path,"%s/save.srm",directory);
    FILE *f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(data,1,n,f)==n;return fclose(f)==0 && ok;
}
void ht_stop(void) { if (loaded && ran_frame) ht_save_data(); if (loaded) retro_unload_game(); loaded = false; memset(buttons, 0, sizeof buttons); ht_audio_clear(); }
void ht_reset(void) { if (loaded) { if (ran_frame) ht_save_data(); retro_reset(); } }
void ht_set_buttons(unsigned p, uint16_t b) { if (p < 4) buttons[p] = b; }
const uint8_t *ht_pixels(void) { return pixels; }
unsigned ht_width(void) { return width; }
unsigned ht_height(void) { return height; }
uint64_t ht_video_serial(void) { return video_serial; }
double ht_fps(void) { return av.timing.fps > 0 ? av.timing.fps : 59.6374; }
double ht_sample_rate(void) { return av.timing.sample_rate > 0 ? av.timing.sample_rate : 48000; }
/* Extra native columns retain the original pixel and object proportions. */
double ht_aspect_ratio(void) { return av.geometry.aspect_ratio > 0 ? av.geometry.aspect_ratio : 4.0/3.0; }
const char *ht_error(void) { return error_text; }
void ht_audio_clear(void) { audio_lock(); memset(&audio_fifo, 0, sizeof audio_fifo); audio_unlock(); }
void ht_audio_read(float *left, float *right, unsigned frames) {
    audio_lock(); ht_fifo_read(&audio_fifo, left, right, frames); audio_unlock();
}
