#include "apic.hpp"
#include <config.hpp>
#include <arch/arch.hpp>
#ifdef CONFIG_FALLBACK_TO_PIT
#	include <drivers/timers/pit/pit.hpp> // fallback if no apic available
#endif
#include <cstdio>

extern "C" volatile uint64_t ticks = 0;

extern "C" void apic_timer_interrupt_handler();
extern "C" uint64_t apic_timer_c_handler(uint64_t rsp) {
	arch::x86_64::cpu::idt::send_eoi(0);

	return rsp; // do not change context
}

void initialise_timer() {
	if (!apic_enabled) return;

	arch::x86_64::cpu::idt::set_descriptor(0xF1, (uint64_t)apic_timer_interrupt_handler, 0x8E);
	arch::x86_64::cpu::idt::send_eoi(0);
}

void give_timer_ticks(uint64_t taken_ticks) {
	(void)taken_ticks;
}

namespace drivers::timers::apic {

void sleep_ms(uint64_t ms) {
	if (!apic_enabled()) {
#ifdef CONFIG_FALLBACK_TO_PIT
		drivers::timers::pit::sleep_ms(ms);
#endif
		return;
	}

	uint64_t curr = ticks;
	uint64_t target = curr + ms;
	asm ("sti");
	while (get_ticks() < target) {
		asm ("hlt");
	}
}

uint64_t ns_elapsed_time() {
	if (!apic_enabled()) {
#ifdef CONFIG_FALLBACK_TO_PIT
		return drivers::timers::pit::ns_elapsed_time();
#else
		return 0;
#endif
	}

	return ticks * 1000000ULL;
}

uint64_t get_ticks() {
	if (!apic_enabled()) {
#ifdef CONFIG_FALLBACK_TO_PIT
		return drivers::timers::pit::get_ticks();
#else
		return 0;
#endif
	}
	
	return ticks;
}

}