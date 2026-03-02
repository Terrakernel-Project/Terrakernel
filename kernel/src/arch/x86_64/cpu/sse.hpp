#ifndef SSE_HPP
#define SSE_HPP 1

#include <cstdint>

namespace arch::x86_64::cpu::sse {

void initialise();

void save(void* addr);
void restore(void* addr);

}

#endif
