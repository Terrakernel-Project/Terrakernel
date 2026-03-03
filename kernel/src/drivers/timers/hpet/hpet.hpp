#pragma once

#include <cstdint>

namespace drivers::timers::hpet {

void initialise();
void sleep_ms(uint64_t ms, bool called_by_apic = false);
void sleep_us(uint64_t us);
uint64_t ns_elapsed_time();
void disable();
uint64_t get_ticks();

}
