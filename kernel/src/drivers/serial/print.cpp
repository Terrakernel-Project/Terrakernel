#include "print.hpp"
#include <config.hpp>
#include <drivers/timers/apic/apic.hpp>
#include <arch/arch.hpp>

void print_time() {
    uint64_t ns = drivers::timers::apic::ns_elapsed_time();
    uint64_t s  = ns / 1000000000;
    uint64_t ms = (ns / 1000000) % 1000;

    printf("\x1b[90m\x1b[48;2;64;64;72m[%05llu.%03llu]\x1b[0m ", s, ms);
}

bool kmsg_done = false;

#ifdef CONFIG_PRINT_INFO
bool __cfg_print_info = true;
#else
bool __cfg_print_info = false;
#endif

#ifdef CONFIG_PRINT_STATUS
bool __cfg_print_status = true;
#else
bool __cfg_print_status = false;
#endif

namespace Log {

    void force_enable() {
        __cfg_print_info = true;
        __cfg_print_status = true;
    }

    void errf(const char* fmt, ...) {
        if (kmsg_done) return;
        print_time();

        printf("\x1b[30;41m ERROR   \x1b[0m ");
        printf("\x1b[31m");

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("\x1b[0m\n\r");
    }

    void err(const char* s) {
        if (kmsg_done) return;
        errf("%s", s);
    }

    void warnf(const char* fmt, ...) {
        if (kmsg_done) return;
        print_time();

        printf("\x1b[30;43m WARNING \x1b[0m ");
        printf("\x1b[33m");

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("\x1b[0m\n\r");
    }

    void warn(const char* s) {
        if (kmsg_done) return;
        warnf("%s", s);
    }

    void infof(const char* fmt, ...) {
        if (kmsg_done) return;
        if (!__cfg_print_info) return;

        print_time();

        printf("\x1b[30;44m INFO    \x1b[0m ");
        printf("\x1b[34m");

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("\x1b[0m\n\r");
    }

    void info(const char* s) {
        if (kmsg_done) return;
        infof("%s", s);
    }

    void printf_status(const char* status, const char* fmt, ...) {
        if (kmsg_done) return;
        if (!__cfg_print_status) return;

        print_time();

        printf("\x1b[30;45m %-7.7s \x1b[0m ", status);
        printf("\x1b[35m");

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);

        printf("\x1b[0m\n\r");
    }

    void print_status(const char* status, const char* s) {
        if (kmsg_done) return;
        printf_status(status, "%s", s);
    }

    void nohlt_panic(const char* message) {
        print_time();

        printf("\x1b[30;41m PANIC! \x1b[0m ");
        printf("\x1b[31m%s\x1b[0m\n\r", message);
    }

    void print_rtc_time(const char* message) {
        if (kmsg_done) return;

        print_time();

        uint8_t hours   = arch::x86_64::cpu::rtc::get_hours();
        uint8_t minutes = arch::x86_64::cpu::rtc::get_minutes();
        uint8_t seconds = arch::x86_64::cpu::rtc::get_seconds();

        if (hours > 12) {
            hours -= 12;
            printf("%s: [H:M:S] %02d:%02d:%02d PM\n\r",
                   message, hours, minutes, seconds);
        } else {
            printf("%s: [H:M:S] %02d:%02d:%02d AM\n\r",
                   message, hours, minutes, seconds);
        }
    }

    void end_kernel_messages() {
        kmsg_done = true;
    }
}
