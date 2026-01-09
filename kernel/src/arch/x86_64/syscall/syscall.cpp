#include "syscall.hpp"
#include <cstdio>
#include "handlers.hpp"
#include <arch/arch.hpp>

#define IA32_EFER 0xC0000080
#define IA32_STAR 0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084

struct syscall_regs {
    uint64_t r9;
    uint64_t r8;
    uint64_t r10;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rax;
    uint64_t r11;
    uint64_t rcx;
};

extern "C" void syscall_func();
extern "C" uint64_t syscall_handler(syscall_regs* context) {
	return handle_syscall(context->rax, context->rdi, context->rsi, context->rdx, context->r10, context->r8, context->r9);
}

namespace arch::x86_64::syscall {

void initialise() {
	uint64_t star = ((uint64_t)0x08 << 32)
    				| ((uint64_t)0x10 << 48);
	
	uint64_t efer = arch::x86_64::misc::rdmsr(IA32_EFER);
	efer |= 1; // syscalls enabled
	arch::x86_64::misc::wrmsr(IA32_EFER, efer);
	arch::x86_64::misc::wrmsr(IA32_STAR, star);
	arch::x86_64::misc::wrmsr(IA32_LSTAR, (uint64_t)&syscall_func);
	arch::x86_64::misc::wrmsr(IA32_FMASK, 0);
}

}
