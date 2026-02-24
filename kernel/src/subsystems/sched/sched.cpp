#include "sched.hpp"
#include <structs/task_context.hpp>
#include <arch/arch.hpp>
#include <stack.hpp>
#include <cstdio>
#include <mem/mem.hpp>

static bool    ready    = false;
static int64_t next_pid = 1;
static int64_t next_tid = 1;

pcb* current_pcb = nullptr;
tcb* running_tcb = nullptr;
pcb  pcb_table[256];
int  curr_pcb = 0;

static int64_t alloc_pid() { return next_pid++; }
static int64_t alloc_tid() { return next_tid++; }

static pcb* alloc_pcb() {
    for (int i = 0; i < 256; i++) {
        if (pcb_table[i].pid == -1) {
            return &pcb_table[i];
        }
    }
    
    return nullptr;
}

static void thread_append(pcb* proc, tcb* t) {
    t->next_thread = nullptr;
    if (!proc->threads) {
        proc->threads = t;
        return;
    }
    tcb* cur = proc->threads;
    while (cur->next_thread) cur = cur->next_thread;
    cur->next_thread = t;
}

static bool has_ready_thread(pcb* p) {
    for (tcb* t = p->threads; t; t = t->next_thread)
        if (t->state == TASK_STATE::READY) return true;
    return false;
}

static inline bool is_ring3(tcb* t) { return (t->ctx.cs & 3) == 3; }

static void ctx_init_kernel(cpu_context& ctx, void (*entry)(), void* stack_top) {
    ctx            = {};
    ctx.rip        = (uint64_t)entry;
    ctx.rsp        = (uint64_t)stack_top;
    ctx.cs         = 0x08;
    ctx.ss         = 0x10;
    ctx.rflags_cpu = 0x202;
}

static void ctx_init_user(cpu_context& ctx, void (*entry)(), void* stack_top) {
    ctx            = {};
    ctx.rip        = (uint64_t)entry;
    ctx.user_rsp   = (uint64_t)stack_top;
    ctx.cs         = 0x1B;
    ctx.ss         = 0x23;
    ctx.rflags_cpu = 0x202;

    ctx.user_rsp += 8*5;
	ctx.rsp = ctx.user_rsp;
}

static void push_empty_ctx(tcb* t) {
    uint64_t* sp = (uint64_t*)t->ctx.rsp;

    sp -= 18;
        
    *--sp = 0x202;
    *--sp = mem::vmm::get_cr3();
    *--sp = (uint64_t)t->ctx.rsp + (8 * 18);

    t->ctx.rsp = (uint64_t)sp;
}

static tcb* make_tcb(void (*entry)(), const char* name, pcb* proc, bool user) {
    void* stack_top = stack_manager_get_new_stack(2, user);
    if (!stack_top) return nullptr;

    tcb* t = (tcb*)mem::heap::calloc(1, sizeof(tcb));
    t->parent_proc   = proc;
    t->next_thread   = nullptr;
    t->tid           = alloc_tid();
    t->state         = TASK_STATE::READY;
    t->exit_code     = 0;
    t->terminate_signal_byte_address = nullptr;

    for (int i = 0; i < 15 && name[i]; i++)
        t->dname[i] = name[i];
    t->dname[15] = '\0';

    if (user) ctx_init_user(t->ctx, entry, stack_top);
    else      ctx_init_kernel(t->ctx, entry, stack_top);

    push_empty_ctx(t);

    return t;
}

pcb* rr_advance_proc() {
    if (!current_pcb) {
        for (int i = 0; i < 256; i++) {
            if (pcb_table[i].threads && has_ready_thread(&pcb_table[i])) {
                curr_pcb    = i;
                current_pcb = &pcb_table[i];
                return current_pcb;
            }
        }
        return nullptr;
    }
    int start = curr_pcb;
    int idx   = (curr_pcb + 1) % 256;
    while (idx != start) {
        if (pcb_table[idx].threads && has_ready_thread(&pcb_table[idx])) {
            curr_pcb    = idx;
            current_pcb = &pcb_table[idx];
            return current_pcb;
        }
        idx = (idx + 1) % 256;
    }
    if (current_pcb->threads && has_ready_thread(current_pcb))
        return current_pcb;
    return nullptr;
}

tcb* rr_advance_thrd() {
    if (!current_pcb || !current_pcb->threads)
        return nullptr;
    tcb* start = current_pcb->current_thread
                    ? current_pcb->current_thread
                    : current_pcb->threads;
    tcb* t = start->next_thread ? start->next_thread : current_pcb->threads;
    while (t != start) {
        if (t->state == TASK_STATE::READY) {
            current_pcb->current_thread = t;
            return t;
        }
        t = t->next_thread ? t->next_thread : current_pcb->threads;
    }
    if (start->state == TASK_STATE::READY) {
        current_pcb->current_thread = start;
        return start;
    }
    return nullptr;
}

uint64_t __sched_yield(uint64_t current_rsp) {
    if (!ready) return current_rsp;

    if (running_tcb) {
        running_tcb->ctx   = *(cpu_context*)current_rsp;
        running_tcb->state = TASK_STATE::READY;
    }

    pcb* next_proc = rr_advance_proc();
    if (!next_proc) return current_rsp;
    current_pcb = next_proc;

    tcb* next_thrd = rr_advance_thrd();
    if (!next_thrd) return current_rsp;
    running_tcb        = next_thrd;
    running_tcb->state = TASK_STATE::RUNNING;

    if (is_ring3(running_tcb)) {
        arch::x86_64::ringctl::execute_ring3(
            (void(*)())running_tcb->ctx.rip,
            (void*)running_tcb->ctx.user_rsp
        );
        __builtin_unreachable();
    }

    return (uint64_t)&running_tcb->ctx;
}

namespace sched {

void initialise() {
    if (ready) return;
    for (int i = 0; i < 256; i++) {
        pcb_table[i].pid            = -1;
        pcb_table[i].parent         = nullptr;
        pcb_table[i].next           = nullptr;
        pcb_table[i].main_thread    = nullptr;
        pcb_table[i].current_thread = nullptr;
        pcb_table[i].threads        = nullptr;
        pcb_table[i].num_threads    = 0;
        pcb_table[i].state          = TASK_STATE::DEAD;
    }
}

void sched_ready() {
    current_pcb = rr_advance_proc();
    if (!current_pcb) return;
    running_tcb = rr_advance_thrd();
    if (!running_tcb) return;
    running_tcb->state = TASK_STATE::RUNNING;
    ready = true;

    if (!ready) {
    	printf("Failed to mark scheduler ready\n\r");
    }
}

tcb* new_thread(void (*entry)(), const char* name, pcb* parent, bool user) {
    pcb* proc = parent ? parent : current_pcb;
    if (!proc) return nullptr;

    tcb* t = make_tcb(entry, name, proc, user);
    if (!t) return nullptr;

    thread_append(proc, t);
    proc->num_threads++;
    return t;
}

pcb* new_process(void (*entry)(), const char* name, bool user) {
    pcb* proc = alloc_pcb();
    if (!proc) return nullptr;

    proc->pid            = alloc_pid();
    proc->parent         = current_pcb;
    proc->next           = nullptr;
    proc->threads        = nullptr;
    proc->current_thread = nullptr;
    proc->main_thread    = nullptr;
    proc->num_threads    = 0;
    proc->state          = TASK_STATE::READY;

    tcb* main = make_tcb(entry, name, proc, user);
    if (!main) {
        proc->pid = -1;
        return nullptr;
    }

    proc->threads        = main;
    proc->main_thread    = main;
    proc->current_thread = main;
    proc->num_threads    = 1;

    return proc;
}

bool kill_process(pcb* target) {
    if (!target || target == current_pcb) return false;
    for (tcb* t = target->threads; t; t = t->next_thread) {
        t->state     = TASK_STATE::DEAD;
        t->exit_code = ~0ULL;
        void* stack_top = is_ring3(t)
            ? (void*)t->ctx.user_rsp
            : (void*)t->ctx.rsp;
        destroy_stack(stack_top);
    }
    target->state        = TASK_STATE::DEAD;
    target->num_threads  = 0;
    target->pid          = -1;
    return true;
}

bool terminate_task(pcb* target) {
    if (!target || target == current_pcb) return false;
    for (tcb* t = target->threads; t; t = t->next_thread) {
        if (t->terminate_signal_byte_address)
            *((volatile uint8_t*)t->terminate_signal_byte_address) = 1;
        else {
            t->state     = TASK_STATE::DEAD;
            t->exit_code = ~0ULL;
        }
    }
    return true;
}

bool terminate_task(tcb* target) {
    if (!target || target == running_tcb) return false;
    if (target->terminate_signal_byte_address)
        *((volatile uint8_t*)target->terminate_signal_byte_address) = 1;
    else {
        target->state     = TASK_STATE::DEAD;
        target->exit_code = ~0ULL;
        if (target->parent_proc)
            target->parent_proc->num_threads--;
    }
    return true;
}

void exit_thread(uint64_t exit_code) {
    if (!running_tcb || !current_pcb) return;
    running_tcb->state     = TASK_STATE::DEAD;
    running_tcb->exit_code = exit_code;
    current_pcb->num_threads--;
    if (current_pcb->num_threads == 0) {
        current_pcb->state = TASK_STATE::DEAD;
        current_pcb->pid   = -1;
    }
    __sched_yield(0);
}

void exit(uint64_t exit_code) {
    if (!current_pcb) return;
    for (tcb* t = current_pcb->threads; t; t = t->next_thread) {
        t->state     = TASK_STATE::DEAD;
        t->exit_code = exit_code;
    }
    current_pcb->state       = TASK_STATE::DEAD;
    current_pcb->num_threads = 0;
    current_pcb->pid         = -1;
    __sched_yield(0);
}

}
