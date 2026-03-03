#include "gfx.hpp"
#include <lib/Flanterm/gfx.h>
#include <mem/mem.hpp>
#include <mem/pmm_private.hpp>
#include <panic.hpp>
#include <limine.h>

extern uint64_t g_scr_width, g_scr_height;

void ppx(uint32_t x, uint32_t y, uint32_t colour) {
	putpx(x, y, colour);
}

uint32_t gpx(uint32_t x, uint32_t y) {
	return getpx(x, y);
}

uint32_t replace_pixel(uint32_t x, uint32_t y, uint32_t new_colour, bool zero_for_empty) {
    uint32_t old = gpx(x, y);
    if (zero_for_empty && new_colour == 0) return old;
    ppx(x, y, new_colour);
    return old;
}

uint64_t fbx() {
    return g_scr_width;
}

uint64_t fby() {
    return g_scr_height;
}
