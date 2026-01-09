#include "ldisc.hpp"
#include "ldisc_qwerty.hpp"
#include <drivers/input/ps2k/ps2k.hpp>
#include <drivers/input/ps2k/ps2k_key_event.hpp>
#include <drivers/input/ps2k/ps2k_keycodes.hpp>
#include <cstdio>

enum line_discipline_mode : int {
	MODE_NORMAL = 0,
	MODE_SHIFT = 1,
	MODE_CTRL = 2,
	MODE_CAPS = 4,
};

inline line_discipline_mode operator~(line_discipline_mode a) {
    return static_cast<line_discipline_mode>(~static_cast<int>(a));
}

inline line_discipline_mode operator|(line_discipline_mode a, line_discipline_mode b) {
    return static_cast<line_discipline_mode>(
        static_cast<int>(a) | static_cast<int>(b)
    );
}

inline line_discipline_mode operator&(line_discipline_mode a, line_discipline_mode b) {
    return static_cast<line_discipline_mode>(
        static_cast<int>(a) & static_cast<int>(b)
    );
}

inline line_discipline_mode operator^(line_discipline_mode a, line_discipline_mode b) {
    return static_cast<line_discipline_mode>(
        static_cast<int>(a) ^ static_cast<int>(b)
    );
}

inline line_discipline_mode& operator|=(line_discipline_mode& a, line_discipline_mode b) {
    return a = a | b;
}

inline line_discipline_mode& operator&=(line_discipline_mode& a, line_discipline_mode b) {
    return a = a & b;
}

inline line_discipline_mode& operator^=(line_discipline_mode& a, line_discipline_mode b) {
    return a = a ^ b;
}

line_discipline_mode mode;

char* read_buf;
size_t read_buf_size;
size_t read_n = 0;
bool reading = false;
bool echo = false;

void reset();

void push(char c) {
	if (!reading) return;
	if (!read_buf) return;
	if (read_n > read_buf_size) read_n = 0; /* ring buffer */
	if (c == '\n') { reset(); return; }
	if (c == '\b') {
		if (read_n == 0) { return; }
		
		read_buf[--read_n] = 0; return;
	}
	read_buf[read_n] = c;
	(*(uint64_t*)(&read_n))++;
}

void reset() {
	reading = false;
	read_buf = 0;
	read_buf_size = 0;
	echo = false;
}

void set(bool __echo, char* __read_buf, size_t __read_buf_size) {
	while (reading); /* prevent it from reading while another procedure is reading */
	read_buf = __read_buf;
	read_buf_size = __read_buf_size;
	read_n = 0;
	reading = true;
	echo = __echo;
}

bool read_done() {
	return !reading;
}

size_t read_count() {
	return read_n;
}

void ldisc_ps2k_event_handler(key_event& ev, void*) {
    switch (ev.keycode) {
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            if (ev.state == key_state::PRESSED)
				mode |= MODE_SHIFT;
            else
                mode &= ~MODE_SHIFT;
            return;

        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL:
            if (ev.state == key_state::PRESSED)
				mode |= MODE_CTRL;
            else
                mode &= ~MODE_CTRL;
            return;

        case KEY_CAPSLOCK:
            if (ev.state == key_state::PRESSED)
               	mode ^= MODE_CAPS;
            return;

        default:
            break;
    }

    if (ev.state != key_state::PRESSED) return;

    char c = 0;
    if (mode & MODE_SHIFT)
        c = drivers::tty::ldisc::qwerty::shifted[ev.keycode];
    else if (mode & MODE_CAPS)
        c = drivers::tty::ldisc::qwerty::capsed[ev.keycode];
    else
        c = drivers::tty::ldisc::qwerty::normal[ev.keycode];

    if (c && check_printable(ev.keycode)) {
        if (echo) putchar(c);
        push(c);
    }
}

ssize_t builtin_ldisc_read(bool echo, void* buf, size_t n) {
	if (!buf || n == 0) return 0;

    set(echo, (char*)buf, n);

    while (!read_done());

    size_t count = read_count();
    reset();
    return count;
}

ssize_t builtin_ldisc_write(void* buffer, size_t num) {
	return printf("%.*s", num, (char*)buffer);
}

uint16_t builtin_char_to_keycode(char c) {
    if (c >= 'A' && c <= 'Z') {
        return static_cast<uint16_t>(KEY_A + (c - 'A'));
    }
    return KEY_NONE;
}

uint16_t builtin_num_to_fn_key(int num) {
	return (uint16_t)((int)KEY_F1 + (num - 1));
}

namespace drivers::tty::ldisc {

void initialise() {
	drivers::input::ps2k::set_event_callback((event_callback_fn)ldisc_ps2k_event_handler, nullptr);
}

ssize_t read(bool echo, char* buf, size_t n) {
	return builtin_ldisc_read(echo, buf, n);
}

ssize_t write(const char* buf, ssize_t n) {
	if (n < 0) return builtin_ldisc_write((void*)buf, (size_t)-1);
	else return builtin_ldisc_write((void*)buf, n);
}

}
