#include "print.hpp"
#include <config.hpp>
#include <drivers/timers/apic/apic.hpp>
#include <arch/arch.hpp>

void print_time() {
    uint64_t ns = drivers::timers::apic::ns_elapsed_time();
    uint64_t s  = ns / 1000000000;
    uint64_t ms = (ns / 1000000) % 1000;

    printf("\x1b[90m[%05llu.%03llu]\x1b[0m ", s, ms);
}

bool kmsg_done = false;

namespace Log {
	void errf(const char* fmt, ...) {
		if (kmsg_done) return;
		print_time();
		printf("[ \x1b[1;31mERROR\x1b[0m ] ");
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		printf("\n\r");
	}

	void err(const char* s) {
		if (kmsg_done) return;
		errf("%s", s);
	}

	void warnf(const char* fmt, ...) {
		if (kmsg_done) return;
		print_time();
		printf("[ \x1b[1;mWARNING\x1b[0m ] ");
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		printf("\n\r");
	}

	void warn(const char* s) {
		if (kmsg_done) return;
		warnf("%s", s);
	}
	
	void infof(const char* fmt, ...) {
		if (kmsg_done) return;
#ifdef CONFIG_PRINT_INFO
		print_time();
		printf("[ \x1b[94mINFO\x1b[0m ]  ");
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		printf("\n\r");
#endif
	}

	void info(const char* s) {
		if (kmsg_done) return;
#ifdef CONFIG_PRINT_INFO
		infof("%s", s);
#endif
	}
	
	void printf_status(const char* status, const char* fmt, ...) {
		if (kmsg_done) return;
#ifdef CONFIG_PRINT_STATUS
		print_time();
		printf("[ \x1b[92m%s\x1b[0m ] ", status);
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		printf("\n\r");
#endif
	}

	void print_status(const char* status, const char* s) {
		if (kmsg_done) return;
#ifdef CONFIG_PRINT_STATUS
		printf_status(status, "%s", s);
#endif
	}

	void panic(const char* message) {
		if (kmsg_done) return;
		print_time();
		printf("[ \x1b[1;31mPANIC!\x1b[0m ] %s\n\r", message);
	}

	void print_rtc_time(const char* message) {
		if (kmsg_done) return;

		uint8_t hours = arch::x86_64::cpu::rtc::get_hours();
		uint8_t minutes = arch::x86_64::cpu::rtc::get_minutes();
		uint8_t seconds = arch::x86_64::cpu::rtc::get_seconds();

		if (hours > 12) {
			hours -= 12;
			printf("%s: [H:M:S] %02d:%02d:%02d PM\n\r", message, hours, minutes, seconds);
		} else {
			printf("%s: [H:M:S] %02d:%02d:%02d AM\n\r", message, hours, minutes, seconds);
		}
	}

	void end_kernel_messages() {
		kmsg_done = true;
	}
}
