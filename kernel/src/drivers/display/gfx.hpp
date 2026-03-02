#ifndef GFX_HPP
#define GFX_HPP 1

#include <cstdint>

void ppx(uint64_t x, uint64_t y, uint32_t colour);
uint32_t gpx(uint64_t x, uint64_t y);
uint32_t replace_pixel(uint64_t x, uint64_t y, uint32_t new_colour, bool zero_for_empty);
uint64_t fbx();
uint64_t fby();

void init_userfb();
void user_fb_frame();
void* get_user_fb();

void userfb_ready();

#endif
