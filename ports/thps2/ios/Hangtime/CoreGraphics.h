#ifndef CORE_GRAPHICS_H
#define CORE_GRAPHICS_H
#include "libretro.h"
bool hw_prepare(struct retro_hw_render_callback *callback);
void hw_begin(void);
void hw_end(void);
void hw_read(uint8_t *pixels,unsigned width,unsigned height);
void hw_release(void);
#endif
