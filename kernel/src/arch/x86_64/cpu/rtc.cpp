#include "rtc.hpp"
#include <arch/arch.hpp>

#define RTC_INDEX_PORT 0x70
#define RTC_DATA_PORT  0x71
#define RTC_SECONDS    0x00
#define RTC_MINUTES    0x02
#define RTC_HOURS      0x04

uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val & 0x0F);
}

namespace arch::x86_64::cpu::rtc {

uint8_t get_seconds() {
    arch::x86_64::io::outb(RTC_INDEX_PORT, RTC_SECONDS);
    return bcd_to_bin(arch::x86_64::io::inb(RTC_DATA_PORT));
}

uint8_t get_minutes() {
    arch::x86_64::io::outb(RTC_INDEX_PORT, RTC_MINUTES);
    return bcd_to_bin(arch::x86_64::io::inb(RTC_DATA_PORT));
}

uint8_t get_hours() {
    arch::x86_64::io::outb(RTC_INDEX_PORT, RTC_HOURS);
    return bcd_to_bin(arch::x86_64::io::inb(RTC_DATA_PORT));
}

bool is_night() {
    uint8_t hours = get_hours();
    return hours >= 18 || hours < 6;
}

bool is_day() {
    return !is_night();
}

bool is_am() {
    uint8_t hours = get_hours();
    return hours < 12;
}

bool is_pm() {
    return !is_am();
} // idk why i have a converse for those funcs but maybe to not write !is_xyz() everywhere :> :meme:

}