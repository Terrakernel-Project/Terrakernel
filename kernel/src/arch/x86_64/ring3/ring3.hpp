#ifndef RING3_HPP
#define RING3_HPP 1

#include <arch/arch.hpp>

namespace arch::x86_64::ringctl {

extern "C" void execute_ring3(void (*entry)(), void* stack_base, uint64_t a = 0, uint64_t b = 0, uint64_t c = 0, uint64_t d = 0);

}

#endif
