#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

void initialise_console();
Handle* get_conw();
Handle* get_conr();
void conprint(const char* __restrict s);
void conputc(char c);
int64_t conread(char* __restrict buf, size_t count);
