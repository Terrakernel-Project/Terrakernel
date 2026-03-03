#include "console.h"
#include <sys/syscalls.h>
#include <stdint.h>
#include <stddef.h>

Handle* CONW;
Handle* CONR;

void initialise_console() {
	CONW = HlCreateNewHandle();
	CONR = HlCreateNewHandle();

	HlOpenConsole(CONR, CONW);
}

Handle* get_conw() {
	return CONW;
}

Handle* get_conr() {
	return CONR;
}

size_t __internal_strlen(char* s) {
	size_t c = 0;
	while (*s) {
		s++;
		c++;
	}
	return c;
}

void conprint(const char* __restrict s) {
	HlWriteConsole(CONW, s, __internal_strlen(s));
}

void conputc(char c) {
	HlWriteConsole(CONW, &c, 1);
}

int64_t conread(char* __restrict buf, size_t count) {
	return HlReadConsole(CONR, (void*)buf, count);
}
