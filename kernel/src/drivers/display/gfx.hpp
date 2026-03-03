#ifndef GFX_HPP
#define GFX_HPP 1

#include <cstdint>

void ppx(uint32_t x, uint32_t y, uint32_t colour);
uint32_t gpx(uint32_t x, uint32_t y);
uint32_t replace_pixel(uint32_t x, uint32_t y, uint32_t new_colour, bool zero_for_empty);
uint64_t fbx();
uint64_t fby();

#endif
