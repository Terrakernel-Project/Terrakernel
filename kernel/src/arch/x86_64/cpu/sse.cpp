#include "sse.hpp"

namespace arch::x86_64::cpu::sse {

void initialise() {
    uint64_t cr0;
    uint64_t cr4;

    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);
    cr0 |=  (1ULL << 1);
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);
    cr4 |= (1ULL << 10);
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    asm volatile("fninit");

    uint32_t mxcsr = 0x1F80;
    asm volatile("ldmxcsr %0" :: "m"(mxcsr));
}

void save(void* addr) {
    asm volatile("fxsave (%0)" :: "r"(addr) : "memory");
}

void restore(void* addr) {
    asm volatile("fxrstor (%0)" :: "r"(addr));
}

}
