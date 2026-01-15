#ifndef SCHED_HPP
#define SCHED_HPP 1

#include <cstdint>

enum TaskState {
    PROC_EMBRYO,
    PROC_RUNNABLE,
    PROC_SLEEPING,
    PROC_RUNNING,
    PROC_ZOMBIE,
};

struct Task {
    int64_t pid;
    TaskState state;
    int exit_code;
    uint64_t rsp;
};

struct ContextFrame {
    uint64_t cr3;
    uint64_t rflags;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    
    uint64_t rip, cs, cpu_rflags, rsp, ss;
};

namespace tasking::sched {

void initialise();
bool initialised();
Task* create_task(void (*entry)(), bool kernel_task = true);
uint64_t schedule(uint64_t old_rsp);
Task* get_current_task();

}

#endif