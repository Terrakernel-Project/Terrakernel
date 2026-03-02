#ifndef PS2M_HPP
#define PS2M_HPP 1

#include <cstdint>
#include <cstddef>

struct MousePoint {
	int x, y;
};

namespace drivers::input::ps2m {

void initialise();
MousePoint get_mouse_position();
bool get_mouse_lmb_state();
bool get_mouse_mmb_state();
bool get_mouse_rmb_state();

}

#endif
