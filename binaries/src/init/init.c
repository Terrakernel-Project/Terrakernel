#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>
#include "console.h"

HlConsoleStat* constat;

void HlMain(void) {
	initialise_console();

	constat = (HlConsoleStat*)HlMemoryPoolAllocate(sizeof(HlConsoleStat));

	HlStatConsole(get_conw(), constat);

	if (constat->supports_colour) {
		conprint("SUPPORT COLOUR\r\n");
	}

	if (constat->supports_unicode) {
		conprint("HOW TF BUT SUPPORT UNICODE\r\n");
	}

	if (constat->supports_mouse) {
		conprint("KEWL IT SUPPORTS MOUSE\r\n");
	}

	if (constat->supports_resize) {
		conprint("SUPPORT RESIZE\r\n");
	}

	HlSleepMs(2500);

	conprint("\033[2J\033[H");

	const char* title = "TEST TUI";
	const int titlelen = 8;
	
	for (uint32_t y = 0; y < constat->height_cells; y++) {
		uint32_t x;
		if (y == 0) {
			for (x = 0; x < constat->width_cells; x++) {
				if (x == 0 || x == constat->width_cells - 1) {
					conprint("#");
				} else {
					int title_start = (constat->width_cells - titlelen) / 2;
					int title_end = title_start + titlelen;
					if (x >= title_start && x < title_end) {
						conputc(title[x - title_start]);
					} else {
						conprint(" ");
					}
				}
			}
		} else if (y == constat->height_cells - 1) {
			for (x = 0; x < constat->width_cells; x++) {
				conprint("#");
			}
		} else {
			for (x = 0; x < constat->width_cells; x++) {
				if (x == 0 || x == constat->width_cells - 1) {
					conprint("#");
				} else {
					conprint(" ");
				}
			}
		}
		conprint("\r\n");
	}

    while (1);
}
