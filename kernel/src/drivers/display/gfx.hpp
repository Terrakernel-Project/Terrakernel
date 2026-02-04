#ifndef GFX_HPP
#define GFX_HPP

#include <lib/Flanterm/gfx.h>
#include <cstdint>

// This is the only driver with no namespace, because I think I will use gfx.hpp a lot

static inline void ppx(uint64_t x, uint64_t y, uint32_t colour) {
    putpx(x, y, colour);
}

static inline uint32_t gpx(uint64_t x, uint64_t y) {
    return getpx(x, y);
}

static inline uint32_t replace_pixel(uint64_t x, uint64_t y, uint32_t new_colour, bool zero_for_empty = false) {
    if (zero_for_empty && new_colour == 0) return gpx(x, y);
    uint32_t colour = gpx(x, y);
    ppx(x, y, new_colour);
    return colour;
}

extern uint64_t g_scr_width, g_scr_height;

static inline uint64_t fbx() {
	return g_scr_width;
}

static inline uint64_t fby() {
	return g_scr_height;
}

#endif
