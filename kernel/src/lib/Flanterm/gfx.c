#include "flanterm.h"
#include "flanterm_backends/fb.h"
#include <limine.h>
#include "gfx.h"
#include <drivers/serial/printf.h>
#include <config.hpp>

__attribute__((section(".limine_requests")))
volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// ======= Flanterm theme, just for looks (kernel log, pre-TTY) =======
static uint32_t ft_ansi_colours[8] = {
	0x00101114, /* black */
	0x008b2e2e, /* red */
	0x003a7d44, /* green */
	0x008b6f1f, /* yellow */
	0x002f4f7f, /* blue */
	0x006a3f7a, /* magenta */
	0x002f6f73, /* cyan */
	0x00c5c8c6, /* white */
};

static uint32_t ft_ansi_bright_colours[8] = {
	0x00202024, /* bright black (dark gray) */
	0x00b84a4a, /* bright red */
	0x004fae66, /* bright green */
	0x00b8952e, /* bright yellow */
	0x004c78c4, /* bright blue */
	0x008c5fbf, /* bright magenta */
	0x004fb3b8, /* bright cyan */
	0x00e6e6e6, /* bright white */
};
static uint32_t ft_default_bg = 0x000d0f12; /* near-black */
static uint32_t ft_default_fg = 0x00d0d0d0; /* neutral light gray */
static uint32_t ft_default_bg_bright = 0x0015151a; /* slightly lifted */
static uint32_t ft_default_fg_bright = 0x00ffffff; /* clean white */

static struct limine_framebuffer* fb;

struct flanterm_context *tty;

uint64_t g_scr_height, g_scr_width;

void flanterm_initialise() {
    if (fb_request.response == (void*)0 || fb_request.response->framebuffer_count < 1) {
        asm volatile ("hlt;cli;");
    }

    fb = fb_request.response->framebuffers[0];

    tty = flanterm_fb_init(
		NULL,
	    NULL,
	    fb->address, fb->width, fb->height, fb->pitch,
	    fb->red_mask_size, fb->red_mask_shift,
	    fb->green_mask_size, fb->green_mask_shift,
	    fb->blue_mask_size, fb->blue_mask_shift,
	    NULL,
	    ft_ansi_colours, ft_ansi_bright_colours,
	    &ft_default_bg, &ft_default_fg,
	    &ft_default_bg_bright, &ft_default_fg_bright,
	    NULL, 0, 0, 1,
	    0, 0,
	    5
	);

    g_scr_height = fb->height;
    g_scr_width = fb->width;
}

__attribute__((hot))
uint32_t lerpRGB(uint32_t src, uint32_t dst, uint8_t intensity) {
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >>  8) & 0xFF;
    uint32_t sb =  src        & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >>  8) & 0xFF;
    uint32_t db =  dst        & 0xFF;

    uint32_t r = sr + ((int32_t)(dr - sr) * intensity >> 8);
    uint32_t g = sg + ((int32_t)(dg - sg) * intensity >> 8);
    uint32_t b = sb + ((int32_t)(db - sb) * intensity >> 8);

    return (r << 16) | (g << 8) | b;
}

__attribute__((hot))
void putpx(int x, int y, uint32_t colour) {
	if (x > g_scr_width-1 || y > g_scr_height-1 || x < 0 || y < 0) return;
	//uint32_t currcolour = ((volatile uint32_t *)fb->address)[y * (fb->pitch/4) + x];
	//uint32_t final = colour & 0xFFFFFF/*lerpRGB(currcolour, colour, (colour >> 24) & 0xFF)*/;
	((volatile uint32_t *)fb->address)[y * (fb->pitch/4) + x] = colour;
}

__attribute__((always_inline, hot))
uint32_t getpx(int x, int y) {
	if (x > g_scr_width-1 || y > g_scr_height-1 || x < 0 || y < 0) return (uint32_t)-1;
	return ((volatile uint32_t *)fb->address)[y * (fb->pitch/4) + x];
}

void* get_ftctx() {return (void*)tty;}

void fb_clrscr(int lazy_to_remove) {
    flanterm_write(tty, "\033[2J\033[H", 7);

    size_t pixels = (fb->pitch * fb->height) / 4;
    volatile uint32_t* buf = (volatile uint32_t*)fb->address;

    for (size_t i = 0; i < pixels; i++) {
        buf[i] = 0;
    }
}

uint64_t get_base_fb() {
    return (uint64_t)fb->address;
}

uint64_t get_pitch() {
    return fb->pitch;
}

uint64_t get_bpp() {
    return fb->bpp;
}

uint64_t get_stride() {
    return fb->pitch / (fb->bpp / 8);
}

struct limine_framebuffer* get_fb() {
    return fb;
}

void refresh_tty() {
	flanterm_full_refresh(tty);
}
