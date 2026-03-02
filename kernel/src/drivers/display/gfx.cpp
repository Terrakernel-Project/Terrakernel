#include "gfx.hpp"
#include <lib/Flanterm/gfx.h>
#include <mem/mem.hpp>
#include <mem/pmm_private.hpp>
#include <panic.hpp>
#include <limine.h>

volatile uint32_t* user_fb;
bool user_fb_on = false;

extern uint64_t g_scr_width, g_scr_height;

void ppx(uint64_t x, uint64_t y, uint32_t colour) {
	if (user_fb_on) {
		user_fb[y * g_scr_width + x] = colour;
	}
	putpx(x, y, colour);
}

uint32_t gpx(uint64_t x, uint64_t y) {
	if (user_fb_on) {
		return user_fb[y * g_scr_width + x];
	}
	return getpx(x, y);
}

uint32_t replace_pixel(uint64_t x, uint64_t y, uint32_t new_colour, bool zero_for_empty) {
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

void init_userfb() {
	user_fb = (volatile uint32_t*)mem::pmm::palloc(fb_size_pages());
	mem::vmm::mmap((void*)user_fb, (void*)user_fb, fb_size_pages(), PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_WC);
	mem::memset((void*)user_fb, 0, g_scr_height * get_pitch());
	if (!user_fb) {
		panic("no memory");
	}
}

static bool user_fb_ready = false;

void userfb_ready() {
	user_fb_ready = true;
}

void* sse_memcpy(void* dst, const void* src, uint64_t size) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    uint64_t i = 0;

    for (; i + 16 <= size; i += 16) {
        asm volatile (
            "movdqu (%0), %%xmm0\n\t"
            "movdqu %%xmm0, (%1)\n\t"
            :
            : "r"(s + i), "r"(d + i)
            : "memory"
        );
    }

    for (; i < size; i++) {
        d[i] = s[i];
    }

    return dst;
}

void user_fb_frame() {
	if (user_fb_ready)
		sse_memcpy((void*)get_base_fb(), mem::vmm::pa_to_va((void*)user_fb), g_scr_height * get_pitch());
}

void* get_user_fb() {
	return (void*)user_fb;
}
