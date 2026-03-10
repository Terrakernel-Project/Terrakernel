#include "gfx.hpp"
#include <lib/Flanterm/gfx.h>
#include <mem/mem.hpp>
#include <mem/pmm_private.hpp>
#include <panic.hpp>
#include <limine.h>

extern uint64_t g_scr_width, g_scr_height;
uint64_t fb_size;

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

extern uint64_t g_scr_width, g_scr_height;
volatile uint32_t* cursor_layer;
volatile uint32_t* foreground_layer;
volatile uint32_t* background_layer;
volatile uint32_t* tty_layer;
struct DirtyRect { uint32_t x0, y0, x1, y1; bool valid; };
constexpr size_t MAX_DIRTY_RECTS = 64;
DirtyRect dirty_rects[MAX_DIRTY_RECTS];
size_t dirty_count = 0;

void init_graphics() {
	fb_size = fby()*fbx()*4;
	cursor_layer = (volatile uint32_t*)mem::vmm::valloc((fb_size + 0xFFF) / 0x1000);
	foreground_layer = (volatile uint32_t*)mem::vmm::valloc((fb_size + 0xFFF) / 0x1000);
	background_layer = (volatile uint32_t*)mem::vmm::valloc((fb_size + 0xFFF) / 0x1000);
	
	mem::vmm::mmap(mem::vmm::va_to_pa((void*)cursor_layer), (void*)cursor_layer, (fb_size + 0xFFF) / 0x1000, PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WC);
	mem::vmm::mmap(mem::vmm::va_to_pa((void*)foreground_layer), (void*)foreground_layer, (fb_size + 0xFFF) / 0x1000, PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WC);
	mem::vmm::mmap(mem::vmm::va_to_pa((void*)background_layer), (void*)background_layer, (fb_size + 0xFFF) / 0x1000, PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WC);
	
	mem::memcpy((void*)background_layer, (void*)get_base_fb(), fb_size);
}

inline uint32_t alpha_blend(uint32_t src, uint32_t dst) {
	uint32_t src_a = (src >> 24) & 0xFF;
	uint32_t src_r = (src >> 16) & 0xFF;
	uint32_t src_g = (src >> 8) & 0xFF;
	uint32_t src_b = src & 0xFF;
	
	uint32_t dst_r = (dst >> 16) & 0xFF;
	uint32_t dst_g = (dst >> 8) & 0xFF;
	uint32_t dst_b = dst & 0xFF;
	
	uint32_t inv_a = 255 - src_a;
	
	uint32_t out_r = (src_r * src_a + dst_r * inv_a) / 255;
	uint32_t out_g = (src_g * src_a + dst_g * inv_a) / 255;
	uint32_t out_b = (src_b * src_a + dst_b * inv_a) / 255;
	
	return 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
}

void layerblend_sse(uint32_t* dst, uint32_t* bg, uint32_t* fg, uint32_t* cursor, size_t pixels) {
	for (size_t i = 0; i < pixels; i++) {
		uint32_t pix = bg[i];

		if ((fg[i] >> 24) > 0) {
			pix = alpha_blend(fg[i], pix);
		}
		
		if ((cursor[i] >> 24) > 0) {
			pix = alpha_blend(cursor[i], pix);
		}
		
		dst[i] = pix;
	}
}

void gfx_frame_composit() {
	for (size_t d = 0; d < dirty_count; d++) {
		DirtyRect &r = dirty_rects[d];
		if (!r.valid) continue;
		for (uint32_t y = r.y0; y <= r.y1; y++) {
			size_t offset = y * g_scr_width + r.x0;
			size_t pixels = r.x1 - r.x0 + 1;
			layerblend_sse(
				&((uint32_t*)get_base_fb())[offset],
				&((uint32_t*)background_layer)[offset],
				&((uint32_t*)foreground_layer)[offset],
				&((uint32_t*)cursor_layer)[offset],
			    pixels
			);
		}
		r.valid = false;
	}
	dirty_count = 0;
}

inline void mark_dirty_rect(uint32_t x, uint32_t y) {
	if (dirty_count > 0 && dirty_rects[dirty_count-1].valid) {
		DirtyRect &r = dirty_rects[dirty_count-1];
		if (x >= r.x0 && x <= r.x1+1 && y >= r.y0 && y <= r.y1+1) {
			if (x < r.x0) r.x0 = x;
			if (x > r.x1) r.x1 = x;
			if (y < r.y0) r.y0 = y;
			if (y > r.y1) r.y1 = y;
			return;
		}
	}
	if (dirty_count < MAX_DIRTY_RECTS) {
		dirty_rects[dirty_count] = {x, y, x, y, true};
		dirty_count++;
	}
}

void ppx_cl(uint32_t x, uint32_t y, uint32_t colour) {
	if (x >= g_scr_width || y >= g_scr_height) return;
	cursor_layer[y * g_scr_width + x] = colour;
	mark_dirty_rect(x, y);
}

void ppx_fl(uint32_t x, uint32_t y, uint32_t colour) {
	if (x >= g_scr_width || y >= g_scr_height) return;
	foreground_layer[y * g_scr_width + x] = colour;
	mark_dirty_rect(x, y);
}

void ppx_bl(uint32_t x, uint32_t y, uint32_t colour) {
	if (x >= g_scr_width || y >= g_scr_height) return;
	background_layer[y * g_scr_width + x] = colour;
	mark_dirty_rect(x, y);
}

volatile uint32_t* get_layer_cl() {
	return cursor_layer;
}

volatile uint32_t* get_layer_fg() {
	return foreground_layer;
}

volatile uint32_t* get_layer_bg() {
	return background_layer;
}
