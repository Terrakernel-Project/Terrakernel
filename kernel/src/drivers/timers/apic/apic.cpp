#include "apic.hpp"
#include <arch/arch.hpp>
#include <tasking/sched.hpp>

uint64_t ticks = 0;

extern "C" void apic_timer_interrupt_handler();
extern "C" uint64_t apic_c_timer_interrupt_handler(uint64_t old_rsp) {
    arch::x86_64::cpu::idt::send_eoi(0);
    ticks++;

    return tasking::sched::schedule(old_rsp);
}

void initialise_timer() {
	arch::x86_64::cpu::idt::set_descriptor(0xF1, (uint64_t)apic_timer_interrupt_handler, 0x8E);
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
