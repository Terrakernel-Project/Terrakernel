#ifndef RTC_HPP
#define RTC_HPP 1

#include <cstdint>

namespace arch::x86_64::cpu::rtc {

uint8_t get_seconds();
uint8_t get_minutes();
uint8_t get_hours();
bool is_night();
bool is_day();
bool is_am();
bool is_pm();

}

#endif