#ifndef PS2K_KEYCODES_HPP
#define PS2K_KEYCODES_HPP 1
#include <cstdint>

enum keycode : uint16_t {
    KEY_NONE       = 0,
    KEY_ESC        = 1,
    KEY_1          = 2,
    KEY_2          = 3,
    KEY_3          = 4,
    KEY_4          = 5,
    KEY_5          = 6,
    KEY_6          = 7,
    KEY_7          = 8,
    KEY_8          = 9,
    KEY_9          = 10,
    KEY_0          = 11,
    KEY_MINUS      = 12,
    KEY_EQUAL      = 13,
    KEY_BACKSPACE  = 14,
    KEY_TAB        = 15,
    KEY_Q          = 16,
    KEY_W          = 17,
    KEY_E          = 18,
    KEY_R          = 19,
    KEY_T          = 20,
    KEY_Y          = 21,
    KEY_U          = 22,
    KEY_I          = 23,
    KEY_O          = 24,
    KEY_P          = 25,
    KEY_LEFTBRACE  = 26,
    KEY_RIGHTBRACE = 27,
    KEY_ENTER      = 28,
    KEY_LEFTCTRL   = 29,
    KEY_A          = 30,
    KEY_S          = 31,
    KEY_D          = 32,
    KEY_F          = 33,
    KEY_G          = 34,
    KEY_H          = 35,
    KEY_J          = 36,
    KEY_K          = 37,
    KEY_L          = 38,
    KEY_SEMICOLON  = 39,
    KEY_APOSTROPHE = 40,
    KEY_GRAVE      = 41,
    KEY_LEFTSHIFT  = 42,
    KEY_BACKSLASH  = 43,
    KEY_Z          = 44,
    KEY_X          = 45,
    KEY_C          = 46,
    KEY_V          = 47,
    KEY_B          = 48,
    KEY_N          = 49,
    KEY_M          = 50,
    KEY_COMMA      = 51,
    KEY_DOT        = 52,
    KEY_SLASH      = 53,
    KEY_RIGHTSHIFT = 54,
    KEY_KPASTERISK = 55,
    KEY_LEFTALT    = 56,
    KEY_SPACE      = 57,
    KEY_CAPSLOCK   = 58,
    KEY_F1         = 59,
    KEY_F2         = 60,
    KEY_F3         = 61,
    KEY_F4         = 62,
    KEY_F5         = 63,
    KEY_F6         = 64,
    KEY_F7         = 65,
    KEY_F8         = 66,
    KEY_F9         = 67,
    KEY_F10        = 68,
    KEY_F11        = 69,
    KEY_F12        = 70,
    KEY_NUMLOCK    = 71,
    KEY_SCROLLLOCK = 72,
    KEY_KP7        = 73,
    KEY_KP8        = 74,
    KEY_KP9        = 75,
    KEY_KPMINUS    = 76,
    KEY_KP4        = 77,
    KEY_KP5        = 78,
    KEY_KP6        = 79,
    KEY_KPPLUS     = 80,
    KEY_KP1        = 81,
    KEY_KP2        = 82,
    KEY_KP3        = 83,
    KEY_KP0        = 84,
    KEY_KPDOT      = 85,
    KEY_KPENTER    = 86,
    KEY_RIGHTCTRL  = 87,
    KEY_KPSLASH    = 88,
    KEY_RIGHTALT   = 89,
    KEY_HOME       = 90,
    KEY_UP         = 91,
    KEY_PAGEUP     = 92,
    KEY_LEFT       = 93,
    KEY_RIGHT      = 94,
    KEY_END        = 95,
    KEY_DOWN       = 96,
    KEY_PAGEDOWN   = 97,
    KEY_INSERT     = 98,
    KEY_DELETE     = 99,
    KEY_MUTE       = 100,
    KEY_VOLUMEDOWN = 101,
    KEY_VOLUMEUP   = 102,
    KEY_POWER      = 103,
    KEY_KPEQUAL    = 104,
    KEY_PAUSE      = 105,
    KEY_KPCOMMA    = 106,
    KEY_LEFTMETA   = 107,
    KEY_RIGHTMETA  = 108,
    KEY_COMPOSE    = 109,
    KEY_SLEEP      = 110,
    KEY_WAKEUP     = 111,
    KEY_MEDIA_PLAYPAUSE  = 112,
    KEY_MEDIA_STOP       = 113,
    KEY_MEDIA_PREV       = 114,
    KEY_MEDIA_NEXT       = 115,
    KEY_MEDIA_MUTE       = 116,
    KEY_MEDIA_VOLUMEUP   = 117,
    KEY_MEDIA_VOLUMEDOWN = 118,
};

inline bool check_printable(uint16_t kc) {
	switch (kc) {
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
		case KEY_0:
		case KEY_MINUS:
		case KEY_EQUAL:
		case KEY_BACKSPACE:
		case KEY_TAB:
		case KEY_Q:
		case KEY_W:
		case KEY_E:
		case KEY_R:
		case KEY_T:
		case KEY_Y:
		case KEY_U:
		case KEY_I:
		case KEY_O:
		case KEY_P:
		case KEY_LEFTBRACE:
		case KEY_RIGHTBRACE:
		case KEY_ENTER:
		case KEY_A:
		case KEY_S:
		case KEY_D:
		case KEY_F:
		case KEY_G:
		case KEY_H:
		case KEY_J:
		case KEY_K:
		case KEY_L:
		case KEY_SEMICOLON:
		case KEY_APOSTROPHE:
		case KEY_GRAVE:
		case KEY_BACKSLASH:
		case KEY_Z:
		case KEY_X:
		case KEY_C:
		case KEY_V:
		case KEY_B:
		case KEY_N:
		case KEY_M:
		case KEY_COMMA:
		case KEY_DOT:
		case KEY_SLASH:
		case KEY_KPASTERISK:
		case KEY_SPACE:
		case KEY_KP7:
		case KEY_KP8:
		case KEY_KP9:
		case KEY_KPMINUS:
		case KEY_KP4:
		case KEY_KP5:
		case KEY_KP6:
		case KEY_KPPLUS:
		case KEY_KP1:
		case KEY_KP2:
		case KEY_KP3:
		case KEY_KP0:
		case KEY_KPDOT:
		case KEY_KPENTER:
		case KEY_KPSLASH:
			return true;
		default:
			return false;
	}
	return false;
}

#endif
