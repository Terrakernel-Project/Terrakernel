#ifndef PRINT_HPP
#define PRINT_HPP 1

#include "printf.h"

void print_time();

namespace Log {
	void force_enable();
	void errf(const char* fmt, ...);
	void err(const char* s);
	void warnf(const char* fmt, ...);
	void warn(const char* s);
	void infof(const char* fmt, ...);
	void info(const char* s);
	void printf_status(const char* status, const char* fmt, ...);
	void print_status(const char* status, const char* s);
	// This panic implementation doesn't halt unlike <panic.hpp>:panic(const char*)
	void nohlt_panic(const char* message);
	
	#define putc putchar

	void print_rtc_time(const char* message = "");

	void end_kernel_messages();
}

#endif /* PRINT_HPP */
