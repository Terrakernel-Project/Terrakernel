#ifndef SYSCALL_HPP
#define SYSCALL_HPP 1

#include <arch/arch.hpp>
#include <types.hpp>
#include <cstdint>

struct syscall_entry {
    int num;
    uint64_t* func;
};

namespace arch::x86_64::syscall {

void initialise();

}

#endif
