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
	} else conprint("NO COLOUR\r\n");

	if (constat->supports_unicode) {
		conprint("HOW TF BUT SUPPORT UNICODE\r\n");
	} else conprint("NO UNICODE\r\n");

	if (constat->supports_mouse) {
		conprint("KEWL IT SUPPORTS MOUSE\r\n");
	} else conprint("NO MOUSR :<\r\n");

	if (constat->supports_resize) {
		conprint("SUPPORT RESIZE\r\n");
	} else conprint("NO RESIZE\r\n");	

    while (1);
}
