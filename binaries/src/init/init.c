#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

int main() {
	char* message = "hello\r\n";

	long ret;
	__asm__ volatile (
	    "syscall"
	    : "=a"(ret)
	    : "a"(0),
	      "D"(message)
	    : "rcx", "r11", "memory"
	);

	printf("test\r\n");

    while (1);
}
