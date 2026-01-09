#ifndef PRINT_HPP
#define PRINT_HPP 1

#include "printf.h"

namespace Log {
	void errf(const char* fmt, ...);
	void err(const char* s);
	void warnf(const char* fmt, ...);
	void warn(const char* s);
	void infof(const char* fmt, ...);
	void info(const char* s);
	void printf_status(const char* status, const char* fmt, ...);
	void print_status(const char* status, const char* s);
	// This panic implementation doesn't halt unlike <panic.hpp>:panic(const char*)
	void panic(const char* message);
	void putc(char c);
}

#endif /* PRINT_HPP */
