#include "apic.hpp"
#include <arch/arch.hpp>

uint64_t ticks = 0;

__attribute__((interrupt))
void apic_interrupt_handler(void*) {
	ticks++;

	arch::x86_64::cpu::idt::send_eoi(0);
}

void initialise_timer() {
	arch::x86_64::cpu::idt::set_descriptor(0xF1, (uint64_t)apic_interrupt_handler, 0x8E);
	arch::x86_64::cpu::idt::send_eoi(0);
}

void give_timer_ticks(uint64_t taken_ticks) {
	(void)taken_ticks;
}

namespace drivers::timers::apic {

void sleep_ms(uint64_t ms) {
	uint64_t curr = ticks;
	uint64_t target = curr + ms;
	while (ticks < target);
}

uint64_t ns_elapsed_time() {
	return ticks * 1000000000;
}

}
