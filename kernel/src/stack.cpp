#include "stack.hpp"
#include <mem/mem.hpp>
#include <cstdio>
#include <config.hpp>

#define PAGE_SIZE   0x1000
#define GUARD_PAGES 1
#define STACK_START 0x7FFFFFFF0000ULL
#define STACK_VA_FLOOR 0x1000ULL

#ifdef CONFIG_DEBUG_STACK_MGR
#	define TDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define TDPRINTF(fmt, ...)
#endif

struct stack_entry {
    void*        bottom;
    void*        top;
    size_t       npages;
    size_t       nbytes;
    bool         user;
    stack_entry* next;
    stack_entry* free_next;
};

struct stack_table {
    uint64_t     num_stacks;
    stack_entry* first_stack;
    stack_entry* last_stack;
    stack_entry* free_entries;
    stack_entry* free_va;
    uint64_t     current_top;
} stable = {
    0,
    nullptr, nullptr,
    nullptr,
    nullptr,
    STACK_START
};

static void list_append(stack_entry* e) {
    e->next = nullptr;
    if (stable.last_stack) stable.last_stack->next = e;
    else                   stable.first_stack      = e;
    stable.last_stack = e;
    stable.num_stacks++;
}

static bool list_remove(stack_entry* target) {
    stack_entry* prev = nullptr;
    for (stack_entry* c = stable.first_stack; c; c = c->next) {
        if (c == target) {
            if (prev)                   prev->next         = c->next;
            else                        stable.first_stack = c->next;
            if (c == stable.last_stack) stable.last_stack  = prev;
            stable.num_stacks--;
            return true;
        }
        prev = c;
    }
    return false;
}

static void unmap_stack_pages(void* bottom, size_t npages) {
    for (size_t i = 0; i < npages; i++) {
        void*    va = (void*)((uint64_t)bottom + i * PAGE_SIZE);
        uint64_t pa = mem::vmm::va_to_pa((uint64_t)va);
        mem::vmm::munmap(va, 1);
        mem::pmm::free((void*)pa, 1);
    }
}

static size_t map_stack_pages(uint64_t va_base, size_t npages, bool user) {
    for (size_t i = 0; i < npages; i++) {
        void* phys = mem::pmm::palloc(1);
        if (!phys) {
            TDPRINTF("palloc failed at page %zu/%zu for va_base=0x%llX\n\r",
                     i, npages, (unsigned long long)va_base);
            return i;
        }
        void* va = (void*)(va_base + i * PAGE_SIZE);
        mem::vmm::mmap(phys, va, 1, PAGE_PRESENT | PAGE_RW | (user ? PAGE_USER : 0));
        void* kva = (void*)mem::vmm::pa_to_va(reinterpret_cast<uint64_t>(phys));
        mem::memset(kva, 0, PAGE_SIZE);
    }
    return npages;
}

static stack_entry* allocate_stack_entry(size_t num_pages, bool user) {
    {
        stack_entry** pp = &stable.free_va;
        for (stack_entry* c = stable.free_va; c; c = c->free_next) {
            if (c->npages == num_pages) {
                *pp = c->free_next;

                size_t ok = map_stack_pages((uint64_t)c->bottom, num_pages, user);
                if (ok < num_pages) {
                    unmap_stack_pages(c->bottom, ok);
                    c->free_next        = stable.free_entries;
                    stable.free_entries = c;
                    TDPRINTF("partial remap failure (%zu/%zu pages), dropped VA range\n\r",
                             ok, num_pages);
                    return nullptr;
                }

                c->nbytes    = num_pages * PAGE_SIZE;
                c->user      = user;
                c->next      = nullptr;
                c->free_next = nullptr;
                list_append(c);
                TDPRINTF("reused VA range bottom=0x%llX top=0x%llX\n\r",
                         (unsigned long long)c->bottom, (unsigned long long)c->top);
                return c;
            }
            pp = &c->free_next;
        }
    }

    stack_entry* e = nullptr;
    if (stable.free_entries) {
        e = stable.free_entries;
        stable.free_entries = e->free_next;
        e->free_next = nullptr;
    } else {
        e = (stack_entry*)mem::heap::malloc(sizeof(stack_entry));
        if (!e) {
            TDPRINTF("failed to allocate stack_entry struct\n\r");
            return nullptr;
        }
    }

    const size_t total_pages = num_pages + 2 * GUARD_PAGES;
    const size_t total_bytes = total_pages * PAGE_SIZE;

    if (stable.current_top < total_bytes ||
        stable.current_top - total_bytes < STACK_VA_FLOOR) {
        TDPRINTF("VA space exhausted: current_top=0x%llX total_bytes=0x%zX\n\r",
                 (unsigned long long)stable.current_top, total_bytes);
        e->free_next        = stable.free_entries;
        stable.free_entries = e;
        return nullptr;
    }

    uint64_t region_bottom = stable.current_top - total_bytes;
    uint64_t stack_bottom  = region_bottom + GUARD_PAGES * PAGE_SIZE;
    uint64_t stack_top     = stack_bottom  + num_pages   * PAGE_SIZE;

    size_t ok = map_stack_pages(stack_bottom, num_pages, user);
    if (ok < num_pages) {
        unmap_stack_pages((void*)stack_bottom, ok);
        e->free_next        = stable.free_entries;
        stable.free_entries = e;
        TDPRINTF("map_stack_pages failed (%zu/%zu pages)\n\r", ok, num_pages);
        return nullptr;
    }

    stable.current_top = region_bottom;

    e->bottom    = (void*)stack_bottom;
    e->top       = (void*)stack_top;
    e->npages    = num_pages;
    e->nbytes    = num_pages * PAGE_SIZE;
    e->user      = user;
    e->next      = nullptr;
    e->free_next = nullptr;
    list_append(e);

    TDPRINTF("new stack: bottom=0x%llX top=0x%llX (%zu pages, %s)\n\r",
             (unsigned long long)stack_bottom,
             (unsigned long long)stack_top,
             num_pages, user ? "user" : "kernel");
    return e;
}

void* stack_manager_get_new_stack(size_t num_pages, bool user) {
    stack_entry* e = allocate_stack_entry(num_pages, user);
    if (!e) {
        TDPRINTF("allocate_stack_entry returned null\n\r");
        return nullptr;
    }
    if ((uint64_t)e->top <= STACK_VA_FLOOR) {
        TDPRINTF("BUG: stack top 0x%llX is in the null-page region!\n\r",
                 (unsigned long long)e->top);
        return nullptr;
    }
    return e->top;
}

bool destroy_stack(void* stack_top) {
    if (!stack_top || (uint64_t)stack_top <= STACK_VA_FLOOR) {
        TDPRINTF("called with invalid stack_top=0x%llX\n\r",
                 (unsigned long long)stack_top);
        return false;
    }

    stack_entry* target = nullptr;
    for (stack_entry* c = stable.first_stack; c; c = c->next) {
        if (c->top == stack_top) { target = c; break; }
    }
    if (!target) {
        TDPRINTF("stack_top=0x%llX not found (double-free?)\n\r",
                 (unsigned long long)stack_top);
        return false;
    }

    list_remove(target);
    unmap_stack_pages(target->bottom, target->npages);

    target->next      = nullptr;
    target->free_next = stable.free_va;
    stable.free_va    = target;

    TDPRINTF("destroyed stack top=0x%llX, VA range parked for reuse\n\r",
             (unsigned long long)stack_top);
    return true;
}
