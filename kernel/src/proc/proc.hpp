#ifndef PROC_HPP
#define PROC_HPP 1

#include <cstdint>
#include <cstddef>
#include <exec/elf.hpp>
#include <types.hpp>

#define PROC_MAX 64

enum ProcessState {
    PROC_UNUSED,
    PROC_READY,
    PROC_RUNNING,
    PROC_WAITING,
    PROC_TERMINATED
};

struct Process {
    pid_t pid;
    ProcessState state;
    void* stack;
    void* heap_base;
    size_t heap_size;
    void* entry_point;
    bool user;
};

namespace proc {

void initialise();

pid_t fork();
pid_t vfork();
int execve(const char* path, int argc, char** argv, char** envp);
void exit(int status);
void schedule();

Process* get_current();
Process* get_process(uint64_t pid);

int brk(void* addr);
void* sbrk(ssize_t n);

}

#endif
