#include "cursor.hpp"
#include <drivers/display/gfx.hpp>
#include "cursor_image.hpp"

uint32_t cursor_buffer[CURSOR_WIDTH][CURSOR_HEIGHT] = {0};
uint64_t prev_x = 0, prev_y = 0;
bool first_render = true;

namespace drivers::display::cursor {

void render(uint64_t x, uint64_t y) {
    if (!first_render) {
        for (size_t xx = prev_x; xx < prev_x + CURSOR_WIDTH; xx++) {
            for (size_t yy = prev_y; yy < prev_y + CURSOR_HEIGHT; yy++) {
                if (cursor_image[yy-prev_y][xx-prev_x] != 0) {
                    ppx_cl(xx, yy, 0x00000000);
                }
            }
        }
    }
    
    first_render = false;
    
    for (size_t xx = x; xx < x + CURSOR_WIDTH; xx++) {
        for (size_t yy = y; yy < y + CURSOR_HEIGHT; yy++) {
            uint32_t pixel = cursor_image[yy-y][xx-x];
            if (pixel != 0) {
                ppx_cl(xx, yy, pixel);
            }
        }
    }
    
    prev_x = x;
    prev_y = y;
}

}
