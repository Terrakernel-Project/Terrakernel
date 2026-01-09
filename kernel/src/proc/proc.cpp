#include "proc.hpp"
#include <mem/mem.hpp>
#include <ramfs/ramfs.hpp>
#include <cstring>
#include <arch/arch.hpp>

namespace proc {

static Process proc_table[PROC_MAX];
static Process* current_proc = nullptr;
static pid_t next_pid = 1;

void initialise() {
    // Clear process table
    for (int i = 0; i < PROC_MAX; i++) {
        proc_table[i].pid = 0;
        proc_table[i].state = PROC_UNUSED;
        proc_table[i].stack = nullptr;
        proc_table[i].heap_base = nullptr;
        proc_table[i].heap_size = 0;
        proc_table[i].entry_point = nullptr;
        proc_table[i].user = false;
    }

    Process* first = &proc_table[0];
    first->pid = 1;
    first->state = PROC_READY;
    first->stack = stack_manager_get_new_stack(2);
    first->heap_base = mem::vmm::valloc(1);
    first->heap_size = 0x1000;
    first->entry_point = nullptr;
    first->user = true;

    current_proc = first;
}

Process* get_current() {
    return current_proc;
}

Process* get_process(uint64_t pid) {
    for (int i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].pid == pid)
            return &proc_table[i];
    }
    return nullptr;
}

static Process* allocate_process() {
    for (int i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            proc_table[i].pid = next_pid++;
            proc_table[i].state = PROC_READY;
            return &proc_table[i];
        }
    }
    return nullptr;
}

static void copy_stack(void* dst, void* src, size_t size) {
    mem::memcpy(dst, src, size);
}

static void copy_heap(Process* dst, Process* src) {
    if (src->heap_base && src->heap_size > 0) {
        dst->heap_base = mem::heap::malloc(src->heap_size);
        if (dst->heap_base) {
            mem::memcpy(dst->heap_base, src->heap_base, src->heap_size);
            dst->heap_size = src->heap_size;
        }
    }
}

pid_t fork() {
    Process* parent = get_current();
    Process* child = allocate_process();
    if (!child) return -1;

    *child = *parent;
    child->pid = next_pid++;

    child->stack = stack_manager_get_new_stack(2, parent->user);
    if (child->stack) {
        copy_stack(child->stack, parent->stack, 2 * 4096);
    }

    copy_heap(child, parent);

    return child->pid;
}

pid_t vfork() {
    Process* parent = get_current();
    Process* child = allocate_process();
    if (!child) return -1;

    *child = *parent;
    child->pid = next_pid++;

    child->stack = parent->stack;
    child->heap_base = parent->heap_base;
    child->heap_size = parent->heap_size;

    return child->pid;
}

int execve(const char* path, int argc, char** argv, char** envp) {
    Process* proc = get_current();
    void* buf = nullptr;
    size_t size = 0;

    int fd = ramfs::open(path, O_RDONLY);
    if (fd < 0) return -1;

    struct stat st;
    if (ramfs::fstat(fd, &st) < 0) {
        ramfs::close(fd);
        return -1;
    }
    size = st.st_size;

    buf = mem::heap::malloc(size);
    if (!buf) {
        ramfs::close(fd);
        return -1;
    }

    if (ramfs::read(fd, buf, size) != (int64_t)size) {
        ramfs::close(fd);
        mem::heap::free(buf);
        return -1;
    }
    ramfs::close(fd);

    if (proc->stack) destroy_stack(proc->stack);
    if (proc->heap_base) mem::heap::free(proc->heap_base);

    proc->stack = stack_manager_get_new_stack(2, true);
    proc->heap_base = nullptr;
    proc->heap_size = 0;

	proc->stack -= 8;
	*(uint64_t*)proc->stack = (uint64_t)argc;
	proc->stack -= 8;
	*(uint64_t*)proc->stack = (uint64_t)argv;
	proc->stack -= 8;
	*(uint64_t*)proc->stack = (uint64_t)envp;

    proc->entry_point = get_elf_entry_point_user(buf, size, proc->stack, &proc->stack);
	
	arch::x86_64::ringctl::execute_ring3((void(*)())proc->entry_point, proc->stack);
    
    return 0;
}

void exit(int status) {
    Process* proc = get_current();
    proc->state = PROC_TERMINATED;

    if (proc->stack) {
        destroy_stack(proc->stack);
        proc->stack = nullptr;
    }
    if (proc->heap_base) {
        mem::heap::free(proc->heap_base);
        proc->heap_base = nullptr;
        proc->heap_size = 0;
    }

    schedule();
}

void schedule() {
    for (int i = 0; i < PROC_MAX; i++) {
        if (proc_table[i].state == PROC_READY) {
            current_proc = &proc_table[i];
            current_proc->state = PROC_RUNNING;
            return;
        }
    }
    while (true) asm volatile("hlt");
}

int brk(void* addr) {
    Process* proc = get_current();
    if (!proc) return -1;

    uintptr_t heap_start = (uintptr_t)proc->heap_base;
    uintptr_t new_end = (uintptr_t)addr;

    if (!proc->heap_base) {
        if (new_end == 0) return 0;
        size_t size = new_end;
        proc->heap_base = mem::heap::malloc(size);
        if (!proc->heap_base) return -1;
        proc->heap_size = size;
        return 0;
    }

    if (new_end < heap_start) {
        return -1;
    }

    size_t new_size = new_end - heap_start;
    void* new_heap = mem::heap::realloc(proc->heap_base, new_size);
    if (!new_heap && new_size > 0) return -1;

    proc->heap_base = new_heap;
    proc->heap_size = new_size;
    return 0;
}

void* sbrk(ssize_t n) {
    Process* proc = get_current();
    if (!proc) return (void*)-1;

    uintptr_t current_end = (uintptr_t)proc->heap_base + proc->heap_size;
    if (n == 0) return (void*)current_end;

    uintptr_t new_end = current_end + n;
    if (brk((void*)new_end) < 0) return (void*)-1;

    return (void*)current_end;
}

}
