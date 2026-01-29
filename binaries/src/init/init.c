#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

Handle* ConW, *ConR;

static inline void print(const char* msg) {
	size_t len = 0;
	while (msg[len] != 0) len++;
	HlWriteConsole(ConW, msg, len);
}

void HlMain(void) {
	ConR = HlCreateNewHandle();
	ConW = HlCreateNewHandle();

	HlOpenConsole(ConR, ConW);

	print("Hello, World!");

	while (1);
}
