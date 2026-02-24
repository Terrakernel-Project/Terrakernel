#include "panic.hpp"
#include <drivers/serial/printf.h>

__attribute__((noreturn))
void _panic(const char* func, const char* error_code) {
    printf("PANIC! %s\nError code: %s\n", func, error_code);

    asm volatile ("cli");

    while (1) {
        asm volatile ("hlt");
    }
}

void assert(bool expected) {
    if (!expected) panic("Assertion failed...\n\r");
}

void assert_specific(bool expected, const char* info) {
    if (!expected) {
        printf("Panicking... %s\n\r", info);
        panic("Assertion failed...\n\r");
    }
}
