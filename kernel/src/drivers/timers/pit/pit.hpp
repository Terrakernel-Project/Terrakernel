#ifndef PIT_HPP
#define PIT_HPP 1

#include <cstdint>
#include <arch/arch.hpp>

__attribute__((interrupt))
void pit_interrupt_handler(void*);

namespace drivers::timers::pit {

void initialise();
void sleep_ms(uint64_t ms);
uint64_t ns_elapsed_time();
void disable();
uint64_t get_ticks();

}

#endif
