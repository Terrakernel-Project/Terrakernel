#ifndef GFX_HPP
#define GFX_HPP 1

#include <cstdint>

extern uint64_t fb_size;

extern volatile uint32_t* cursor_layer;
extern volatile uint32_t* foreground_layer;
extern volatile uint32_t* background_layer;

void ppx(uint32_t x, uint32_t y, uint32_t colour);
uint32_t gpx(uint32_t x, uint32_t y);
uint32_t replace_pixel(uint32_t x, uint32_t y, uint32_t new_colour, bool zero_for_empty);
uint64_t fbx();
uint64_t fby();

void init_graphics();
void gfx_frame_composit();
void ppx_cl(uint32_t x, uint32_t y, uint32_t colour);
void ppx_fl(uint32_t x, uint32_t y, uint32_t colour);
void ppx_bl(uint32_t x, uint32_t y, uint32_t colour);

volatile uint32_t* get_layer_cl();
volatile uint32_t* get_layer_fg();
volatile uint32_t* get_layer_bg();

#endif
