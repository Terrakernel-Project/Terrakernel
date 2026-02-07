#ifndef GFX_H
#define GFX_H 1

#include <stdint.h>
#include <limine.h>

#ifdef __cplusplus
extern "C" {
#endif

void flanterm_initialise();

__attribute__((hot))
void putpx(int x, int y, uint32_t colour);
__attribute__((hot))
uint32_t getpx(int x, int y);

void draw_mouse_pointer(int old_x, int old_y, int x, int y, int button_state_lmb, int button_state_mmb, int button_state_rmb);
void fb_clrscr(int no_cur);

uint64_t get_base_fb();
uint64_t get_pitch();
uint64_t get_bpp();
uint64_t get_stride();
struct limine_framebuffer* get_fb();

void switch_to_tty(int tty);
void refresh_tty();

#ifdef __cplusplus
}
#endif

#endif
