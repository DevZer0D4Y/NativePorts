#ifndef SSF2T_CORE_H
#define SSF2T_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
/* Core calls on main thread; audio consumer can run on the audio thread. */
bool ht_validate_rom(const char *path, char *report, size_t capacity);
bool ht_start(const char *path, const char *directory);
void ht_run(void);
/* Intermediate catch-up frames keep audio/input but skip screen rendering/readback. */
void ht_run_frame(bool present);
/* Call on the core/main thread after a drawable was actually presented. */
void ht_displayed_frame(double timestamp);
void ht_set_widescreen(bool enabled);
bool ht_save_data(void);
void ht_stop(void);
void ht_reset(void);
void ht_set_buttons(unsigned player, uint16_t buttons);
const uint8_t *ht_pixels(void);
/* Changes only when the core supplies new pixels; duplicate vblanks keep it. */
uint64_t ht_video_serial(void);
void ht_render_cpu_time(double milliseconds);
void ht_render_gpu_time(double milliseconds);
unsigned ht_width(void);
unsigned ht_height(void);
double ht_fps(void);
double ht_sample_rate(void);
double ht_aspect_ratio(void);
/* Screen mode with the active Dreamcast pixel aspect applied. */
double ht_display_aspect(bool square_pixels);
const char *ht_error(void);
void ht_audio_read(float *left, float *right, unsigned frames);
void ht_audio_clear(void);
#endif
