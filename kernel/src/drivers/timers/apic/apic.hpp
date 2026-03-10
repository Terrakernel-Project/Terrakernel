#include <arch/x86_64/apic/apic.hpp>
#include <cstdint>

void initialise_timer();
void give_timer_ticks(uint64_t ticks_taken);

struct ApicCpuContext {
    uint64_t rsp;
    uint64_t cr3;
    uint64_t rflags;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
};

namespace drivers::timers::apic {

void sleep_ms(uint64_t ms);
uint64_t ns_elapsed_time();
uint64_t get_ticks();

}
