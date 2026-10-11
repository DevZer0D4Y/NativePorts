#ifndef HANGTIME_XBOX_INPUT_H
#define HANGTIME_XBOX_INPUT_H
#include <stdbool.h>
#include <stdint.h>

struct pad_sample { unsigned down; uint16_t directions; bool start, home; };
/* Decode the documented XINPUT_GAMEPAD state independently of SDL mappings. */
static struct pad_sample xbox_sample(uint16_t buttons, uint8_t lt, uint8_t rt, int16_t x, int16_t y) {
    struct pad_sample sample = {0};
    static const struct { uint16_t bit; unsigned input; } map[] = {
        {0x1000,P_A},{0x2000,P_B},{0x4000,P_X},{0x8000,P_Y},
        {0x0100,P_LB},{0x0200,P_RB},{0x0040,P_L3},{0x0080,P_R3},{0x0020,P_BACK}
    };
    for (unsigned i=0;i<sizeof map/sizeof map[0];i++)
        if (buttons & map[i].bit) sample.down |= 1u << map[i].input;
    if (lt>127) sample.down |= 1u << P_LT;
    if (rt>127) sample.down |= 1u << P_RT;
    if ((buttons & 0x0001) || y>10000) sample.directions |= PAD_UP;
    if ((buttons & 0x0002) || y< -10000) sample.directions |= PAD_DOWN;
    if ((buttons & 0x0004) || x< -10000) sample.directions |= PAD_LEFT;
    if ((buttons & 0x0008) || x>10000) sample.directions |= PAD_RIGHT;
    sample.start=(buttons & 0x0010)!=0;
    return sample;
}
#endif
