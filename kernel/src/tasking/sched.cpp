#include "sched.hpp"
#include <mem/mem.hpp>
#include <exec/elf.hpp>
#include <cstdio>

bool __initialised = false;

struct {
    Task* tasks[1024];
    size_t task_count;
    size_t curr_task;
} task_table;

int64_t pids = 0;

Task* alloc_proc(uint64_t entry, bool is_kernel_task) {
    int64_t pid = pids++;
    if (pid < 0) {
        pids = 1;
        pid = pids++;
    }
    
    Task* t = (Task*)mem::heap::malloc(sizeof(Task));
    if (!t) return nullptr;

    t->pid = pid;
    t->state = PROC_EMBRYO;
    t->exit_code = 0;
    
    uint64_t* sp = (uint64_t*)stack_manager_get_new_stack(
        is_kernel_task ? 2 : 64, 
        !is_kernel_task
    );
    if (!sp) {
        mem::heap::free(t);
        printf("Failed to allocate stack for new task PID=%lld\n", pid);
        return nullptr;
    }

    if (is_kernel_task) {
        sp--;
        *(uint64_t*)sp = 0x08;
        sp--;
        *(uint64_t*)sp = entry;
    } else {
        sp--;
        *(uint64_t*)sp = 0x23;
        sp--;
        *(uint64_t*)sp = (uint64_t)sp - 128;
        sp--;
        *(uint64_t*)sp = 0x1B;
        sp--;
        *(uint64_t*)sp = entry;
    }

    sp -= 15;
    
    sp--;
    *(uint64_t*)sp = 0x202;
    sp--;
    *(uint64_t*)sp = mem::vmm::get_cr3();
    
    t->rsp = (uint64_t)sp;
    t->state = PROC_RUNNABLE;
    
    return t;
}

namespace tasking::sched {

void initialise() {
    for (size_t i = task_table.task_count; i < 1024; i++) // start at task_table.task_count to not overwrite existing tasks, i.e, init task
        task_table.tasks[i] = nullptr;
    task_table.task_count = 0;
    task_table.curr_task = 0;

    __initialised = true;
}

bool initialised() {
    return __initialised;
}

Task* create_task(void (*entry)(), bool kernel_task) {
    Task* t = alloc_proc((uint64_t)entry, kernel_task);
    if (!t) return nullptr;

    task_table.tasks[task_table.task_count++] = t;
    return t;
}

uint64_t schedule(uint64_t old_rsp) {
    if (!__initialised) return 0;
    if (task_table.task_count == 0) return 0;
    
    Task* current = task_table.tasks[task_table.curr_task];
    if (current) {
        current->rsp = old_rsp;
        if (current->state == PROC_RUNNING) {
            current->state = PROC_RUNNABLE;
        }
    }
    
    size_t starting_task = task_table.curr_task;
    size_t next_task = (task_table.curr_task + 1) % task_table.task_count;
    
    while (next_task != starting_task) {
        Task* t = task_table.tasks[next_task];
        
        if (t && t->state == PROC_RUNNABLE) {
            task_table.curr_task = next_task;
            t->state = PROC_RUNNING;
            
            printf("Scheduling: PID %lld -> PID %lld (rsp: 0x%lx -> 0x%lx)\n",
                   current ? current->pid : -1, 
                   t->pid,
                   old_rsp,
                   t->rsp);
            
            return t->rsp;
        }
        
        next_task = (next_task + 1) % task_table.task_count;
    }
    
    if (current) {
        current->state = PROC_RUNNING;
    }
    
    return 0;
}

Task* get_current_task() {
    if (task_table.curr_task < task_table.task_count) {
        return task_table.tasks[task_table.curr_task];
    }
    return nullptr;
}

}