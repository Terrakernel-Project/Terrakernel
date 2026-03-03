#include "apic.hpp"
#include <config.hpp>
#include <arch/arch.hpp>
#ifdef CONFIG_FALLBACK_TO_HPET
#	include <drivers/timers/hpet/hpet.hpp> // fallback if no apic available
#endif
#include <cstdio>

volatile uint64_t ticks = 0;

extern "C" void apic_timer_interrupt_handler();
extern "C" ApicCpuContext* apic_timer_c_handler(ApicCpuContext* ctx) {
	arch::x86_64::cpu::idt::send_eoi(0);

	return ctx;
}

void initialise_timer() {
	if (!apic_enabled()) {
		printf("APIC Timer: APIC not enabled\n\r");
		return;
	}

	arch::x86_64::cpu::idt::set_descriptor(0xF1, (uint64_t)apic_timer_interrupt_handler, 0x8E);
	arch::x86_64::cpu::idt::send_eoi(0);
}

void give_timer_ticks(uint64_t taken_ticks) {
	(void)taken_ticks;
}

namespace drivers::timers::apic {

void sleep_ms(uint64_t ms) {
	if (!apic_enabled()) {
#ifdef CONFIG_FALLBACK_TO_HPET
		drivers::timers::hpet::sleep_ms(ms);
#endif
		return;
	}

	uint64_t curr = ticks;
	uint64_t target = curr + ms;
	asm ("sti");
	while (ticks < target) {
		asm ("pause");
	}
}

uint64_t ns_elapsed_time() {
	if (!apic_enabled()) {
#ifdef CONFIG_FALLBACK_TO_HPET
		return drivers::timers::hpet::ns_elapsed_time();
#else
		return 0;
#endif
	}

	return ticks * 1000000ULL;
}

uint64_t get_ticks() {
	if (!apic_enabled()) {
#ifdef CONFIG_FALLBACK_TO_HPET
		return drivers::timers::hpet::get_ticks();
#else
		return 0;
#endif
	}
	
	return ticks;
}

}
