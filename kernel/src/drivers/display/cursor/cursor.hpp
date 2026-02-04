#ifndef CURSOR_HPP
#define CURSOR_HPP 1

#include <cstdint>
#include <cstddef>

namespace drivers::display::cursor {

void render(uint64_t x, uint64_t y);

}

#endif