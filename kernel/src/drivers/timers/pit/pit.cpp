#include "pit.hpp"
#include <cstdio>
#include <config.hpp>

static uint64_t ticks = 0;

#define CHx_DATA(ch) (0x40 + (ch))
#define CHx_MODE_CMD_REG(ch) (0x43)

__attribute__((interrupt))
void pit_interrupt_handler(void*) {
    ticks++;

    printf("int\n\r");

    arch::x86_64::cpu::idt::send_eoi(0);
}

namespace drivers::timers::pit {

void initialise() {
    using namespace arch::x86_64::io;

#if CONFIG_PIT_FREQUENCY == 0
#	error "CONFIG_PIT_FREQUENCY cannot be 0"
#endif

    outb(CHx_MODE_CMD_REG(0), 0x34);

    uint16_t divisor = 1193180 / CONFIG_PIT_FREQUENCY;
    outb(CHx_DATA(0), static_cast<uint8_t>(divisor & 0xFF));
    io_wait();
    outb(CHx_DATA(0), static_cast<uint8_t>((divisor >> 8) & 0xFF));

    arch::x86_64::cpu::idt::set_descriptor(0x20, (uint64_t)pit_interrupt_handler, 0x8E);
    arch::x86_64::cpu::idt::send_eoi(0);

    printf("CONFIG_PIT_FREQUENCY = %d\n\rdivisor = %d\n\r", CONFIG_PIT_FREQUENCY, divisor);
}

void sleep_ms(uint64_t ms) {
	printf("Preparing for sleep\n\r");
    if (CONFIG_PIT_FREQUENCY == 0) {
    	printf("CONFIG_PIT_FREQUENCY == 0\n\r");
    	return;
    }
    
    uint64_t current_ticks = ticks;
    uint64_t blocking_ticks = (ms * CONFIG_PIT_FREQUENCY) / 1000;
    uint64_t final_ticks = current_ticks + blocking_ticks;
	printf("current_ticks = %llu\n\rblocking_ticks = %llu\n\rfinal_ticks = %llu\n\r", current_ticks, blocking_ticks, final_ticks);

    asm ("sti");
    while (ticks < final_ticks);
}

uint64_t ns_elapsed_time() {
    return ticks * 10000000;
}

void disable() {
	//arch::x86_64::io::outb(CHx_MODE_CMD_REG(0), 0x30);
	//arch::x86_64::io::outb(CHx_DATA(0), 0x00);
	//arch::x86_64::io::outb(CHx_DATA(0), 0x00);	
}

uint64_t get_ticks() {
    return ticks;
}

}
