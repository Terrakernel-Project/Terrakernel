#include <arch/x86_64/apic/apic.hpp>
#include <cstdint>

void initialise_timer();
void give_timer_ticks(uint64_t ticks_taken);

namespace drivers::timers::apic {

void sleep_ms(uint64_t ms);
uint64_t ns_elapsed_time();

}
