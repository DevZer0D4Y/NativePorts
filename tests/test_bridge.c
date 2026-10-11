#include "HangtimeCore.h"
#include "libretro.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static retro_environment_t env;
static retro_video_refresh_t frame;
static retro_audio_sample_t sample;
static retro_audio_sample_batch_t batch;
static retro_input_state_t input;
static int resets, runs, unloads, saves, widescreen;
int sf_save_nvram(void){saves++;return 1;}
void *retro_get_memory_data(unsigned id){return NULL;}
size_t retro_get_memory_size(unsigned id){return 0;}
void retro_set_environment(retro_environment_t cb) { env = cb; }
void retro_set_video_refresh(retro_video_refresh_t cb) { frame = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { sample = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { batch = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { (void)cb; }
void retro_set_input_state(retro_input_state_t cb) { input = cb; }
void retro_init(void) {}
bool retro_load_game(const struct retro_game_info *g) { return strcmp(g->path, "load-failure") != 0; }
void retro_get_system_av_info(struct retro_system_av_info *i) { memset(i,0,sizeof *i); i->timing.fps = 54.7; i->timing.sample_rate = 48000; i->geometry.aspect_ratio = 1.25f; }
void retro_set_controller_port_device(unsigned p, unsigned d) { assert(p < 4 && (d & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD); }
void retro_run(void) { runs++; }
void retro_unload_game(void) { unloads++; }
void retro_reset(void) { resets++; }
bool ht_validate_rom(const char *p, char *r, size_t n) {
    if (!strcmp(p,"invalid")) { snprintf(r,n,"Invalid ROM"); return false; } return true;
}
int main(void) {
    assert(!ht_start("invalid","/tmp")); assert(!strcmp(ht_error(),"Invalid ROM"));
    assert(ht_start("game.zip","/tmp")); assert(!ht_start("game.zip","/tmp"));
    assert(ht_fps() == 54.7 && ht_sample_rate() == 48000 && ht_aspect_ratio() == 4.0/3.0);
    struct retro_log_callback log; assert(env(RETRO_ENVIRONMENT_GET_LOG_INTERFACE,&log)); assert(log.log);
    struct retro_variable options[] = {{"test", "Description; default|second"},{NULL,NULL}};
    assert(env(RETRO_ENVIRONMENT_SET_VARIABLES, options));
    struct retro_variable option = {"test",NULL}; assert(env(RETRO_ENVIRONMENT_GET_VARIABLE,&option)); assert(!strcmp(option.value,"default"));
    ht_set_buttons(0, (1u << 4) | (1u << 8));
    assert(input(0,RETRO_DEVICE_JOYPAD,0,4) == 1 && input(0,RETRO_DEVICE_JOYPAD,0,8) == 1);
    assert(input(0,RETRO_DEVICE_JOYPAD,0,5) == 0 && input(4,RETRO_DEVICE_JOYPAD,0,4) == 0);
    enum retro_pixel_format format = RETRO_PIXEL_FORMAT_RGB565;
    assert(env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&format));
    uint16_t rgb565[] = {0xf800,0x07e0,0x1234,0x001f,0xffff,0x5678};
    frame(rgb565,2,2,6);
    const uint8_t expected[] = {255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
    assert(ht_width() == 2 && ht_height() == 2); assert(!memcmp(ht_pixels(),expected,sizeof expected));
    frame(NULL,2,2,6); assert(!memcmp(ht_pixels(),expected,sizeof expected));
    format = RETRO_PIXEL_FORMAT_0RGB1555; assert(env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&format));
    uint16_t rgb1555[] = {0x7c00,0x03e0,0x001f,0x7fff}; frame(rgb1555,2,2,4);
    assert(!memcmp(ht_pixels(),expected,sizeof expected));
    format = RETRO_PIXEL_FORMAT_XRGB8888; assert(env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&format));
    uint32_t xrgb[] = {0x00ff0000,0x0000ff00,0x000000ff,0x00ffffff}; frame(xrgb,2,2,8);
    assert(!memcmp(ht_pixels(),expected,sizeof expected));
    frame(xrgb,2049,1,8196); assert(ht_width()==2);
    ht_set_buttons(1,1u<<11);assert(input(1,RETRO_DEVICE_JOYPAD,0,11)==1);assert(input(0,RETRO_DEVICE_JOYPAD,0,11)==0);
    ht_set_buttons(3,1u<<8);assert(input(3,RETRO_DEVICE_JOYPAD,0,8)==1);
    int16_t pcm[] = {-32768,32767,16384,-16384}; assert(batch(pcm,2) == 2);
    float left[4], right[4]; ht_audio_read(left,right,4);
    assert(left[0] == -1 && right[0] > 0.999 && left[1] == 0.5 && right[1] == -0.5);
    assert(left[2] == 0 && right[3] == 0);
    sample(100,200); ht_audio_clear(); ht_audio_read(left,right,1); assert(left[0] == 0 && right[0] == 0);
    assert(!ht_save_data()); ht_run(); assert(ht_save_data()); ht_reset(); assert(runs == 1 && resets == 1 && saves == 2);
    ht_stop(); ht_run(); ht_reset(); assert(runs == 1 && resets == 1 && unloads == 1);
    assert(input(0,RETRO_DEVICE_JOYPAD,0,4) == 0);
    assert(!ht_start("load-failure","/tmp")); assert(strlen(ht_error()) > 0);

    puts("Core bridge: RGB formats, row pitch, player isolation, audio and lifecycle passed.");
}
