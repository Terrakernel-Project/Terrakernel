#ifndef SCHED_HPP
#define SCHED_HPP 1

#include <cstdint>
#include <structs/task_context.hpp>

enum class TASK_STATE : uint8_t {
    READY,
    RUNNING,
    BLOCKED,
    SLEEPING,
    ZOMBIE,
    DEAD,
};

struct pcb;

struct tcb {
    pcb* parent_proc;
    tcb* next_thread;
    int64_t tid;
    char dname[16];
    TASK_STATE state;
    uint64_t exit_code;
    cpu_context ctx;
    void* terminate_signal_byte_address;
};

struct pcb {
    int64_t pid;
    pcb* parent;
    pcb* next;
    tcb* main_thread;
    tcb* current_thread;
    tcb* threads;
    uint64_t num_threads;
    TASK_STATE state;
};

uint64_t __sched_yield(uint64_t current_rsp);

namespace sched {

void initialise();
void sched_ready();

tcb* new_thread(void (*entry)(), const char* name, pcb* parent = nullptr, bool user = false);
pcb* new_process(void (*entry)(), const char* name, bool user = true);

bool kill_process(pcb* target);

// terminate politely
bool terminate_task(pcb* target);
bool terminate_task(tcb* target);

void exit_thread(uint64_t exit_code);
void exit(uint64_t exit_code);

}

#endif
