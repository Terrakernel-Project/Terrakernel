#include <mem/mem.hpp>
#include <mem/heap.hpp>
#include <panic.hpp>
#include <cstdio>

void*  heap_base = nullptr;
size_t heap_size = 0;
size_t allocated = 0;

struct heap_block {
    size_t length;
    bool is_free;
    heap_block* prev;
    heap_block* next;
    heap_block* fl_prev;
    heap_block* fl_next;
};

static constexpr int N_BINS = 24;
static heap_block*   bins[N_BINS] = {};

static constexpr size_t MIN_ALIGN = sizeof(void*);

static constexpr size_t MIN_SPLIT = 8;

static inline void* block_to_ptr(heap_block* b) {
    return (void*)((uint8_t*)b + sizeof(heap_block));
}
static inline heap_block* ptr_to_block(void* p) {
    return (heap_block*)((uint8_t*)p - sizeof(heap_block));
}
static inline size_t align_up(size_t n, size_t a) {
    return (n + a - 1) & ~(a - 1);
}

static inline int bin_of(size_t n) {
    if (n < 8) n = 8;
    int bit = 63 - __builtin_clzll((unsigned long long)n);
    int b   = bit - 3;
    if (b < 0)      b = 0;
    if (b >= N_BINS) b = N_BINS - 1;
    return b;
}

static void fl_insert(heap_block* b) {
    int idx = bin_of(b->length);
    b->fl_prev = nullptr;
    b->fl_next = bins[idx];
    if (bins[idx]) bins[idx]->fl_prev = b;
    bins[idx] = b;
}

static void fl_remove(heap_block* b) {
    int idx = bin_of(b->length);
    if (b->fl_prev) b->fl_prev->fl_next = b->fl_next;
    else            bins[idx] = b->fl_next;
    if (b->fl_next) b->fl_next->fl_prev = b->fl_prev;
    b->fl_prev = b->fl_next = nullptr;
}

static void split(heap_block* b, size_t n) {
    if (b->length < n + sizeof(heap_block) + MIN_SPLIT) return;

    heap_block* tail = (heap_block*)((uint8_t*)b + sizeof(heap_block) + n);
    tail->length  = b->length - n - sizeof(heap_block);
    tail->is_free = true;
    tail->prev    = b;
    tail->next    = b->next;
    tail->fl_prev = tail->fl_next = nullptr;
    if (b->next) b->next->prev = tail;
    b->next   = tail;
    b->length = n;
    fl_insert(tail);
}

static heap_block* coalesce(heap_block* b) {
    if (b->next && b->next->is_free) {
        fl_remove(b->next);
        b->length += sizeof(heap_block) + b->next->length;
        b->next    = b->next->next;
        if (b->next) b->next->prev = b;
    }

    if (b->prev && b->prev->is_free) {
        heap_block* p = b->prev;
        fl_remove(p);
        p->length += sizeof(heap_block) + b->length;
        p->next    = b->next;
        if (b->next) b->next->prev = p;
        b = p;
    }
    return b;
}

static heap_block* find_free(size_t n) {
    int start = bin_of(n);

    {
        heap_block* best = nullptr;
        for (heap_block* cur = bins[start]; cur; cur = cur->fl_next) {
            if (cur->length >= n) {
                if (!best || cur->length < best->length) best = cur;
                if (best->length == n) break;
            }
        }
        if (best) { fl_remove(best); return best; }
    }

    for (int b = start + 1; b < N_BINS; b++) {
        if (!bins[b]) continue;
        heap_block* cur = bins[b];
        fl_remove(cur);
        return cur;
    }

    return nullptr;
}

namespace mem::heap {

void initialise() {
    const size_t initial_size = 0x100000000;
    const size_t min_size     = 0x100000;
    size_t divisor = 1;
    heap_base = nullptr;

    while (!heap_base && initial_size / divisor >= min_size) {
        heap_size = initial_size / divisor;
        heap_base = mem::pmm::reserve_heap(heap_size / 0x1000);
        divisor  *= 2;
    }
    if (!heap_base) panic("heap: failed to reserve memory");
    heap_base = (void*)mem::vmm::pa_to_va((uint64_t)heap_base);

    for (int i = 0; i < N_BINS; i++) bins[i] = nullptr;

    heap_block* first = (heap_block*)heap_base;
    first->length  = heap_size - sizeof(heap_block);
    first->is_free = true;
    first->prev = first->next = nullptr;
    first->fl_prev = first->fl_next = nullptr;
    fl_insert(first);
}

void defragment() {
    for (heap_block* cur = (heap_block*)heap_base; cur; cur = cur->next) {
        if (!cur->is_free) continue;
        fl_remove(cur);
        cur = coalesce(cur);
        fl_insert(cur);
    }
}

void* malloc(size_t n) {
    if (!n) return nullptr;
    n = align_up(n, MIN_ALIGN);

    heap_block* b = find_free(n);
    if (!b) { defragment(); b = find_free(n); }
    if (!b) { printf("%zu/%zu\n\r", allocated, heap_size); panic("heap::malloc: out of memory"); }

    split(b, n);
    b->is_free = false;
    allocated += b->length;
    return block_to_ptr(b);
}

void* malloc_aligned(size_t n, size_t alignment) {
    if (!n)                              return nullptr;
    if (!alignment || (alignment & (alignment - 1))) return nullptr;
    if (alignment <= MIN_ALIGN)          return malloc(n);

    n = align_up(n, alignment);

    heap_block* found   = nullptr;
    size_t      found_waste = ~(size_t)0;

    auto try_block = [&](heap_block* cur) {
        uintptr_t data = (uintptr_t)cur + sizeof(heap_block);
        uintptr_t aln  = align_up(data, alignment);
        size_t    pad  = aln - data;

        if (pad != 0 && pad < sizeof(heap_block) + MIN_SPLIT) {
            aln += alignment;
            pad  = aln - data;
        }

        if (cur->length < pad + n) return;
        size_t waste = cur->length - pad - n;
        if (waste < found_waste) { found_waste = waste; found = cur; }
    };

    for (int b = bin_of(n + alignment); b < N_BINS; b++)
        for (heap_block* cur = bins[b]; cur; cur = cur->fl_next)
            try_block(cur);

    for (int b = 0; b < bin_of(n + alignment); b++)
        for (heap_block* cur = bins[b]; cur; cur = cur->fl_next)
            try_block(cur);

    if (!found) { defragment(); /* retry */ }
    if (!found) {
        found_waste = ~(size_t)0;
        for (int b = 0; b < N_BINS; b++)
            for (heap_block* cur = bins[b]; cur; cur = cur->fl_next)
                try_block(cur);
    }
    if (!found) panic("heap::malloc_aligned: out of memory");

    fl_remove(found);

    uintptr_t data = (uintptr_t)found + sizeof(heap_block);
    uintptr_t aln  = align_up(data, alignment);
    size_t    pad  = aln - data;

    if (pad != 0 && pad < sizeof(heap_block) + MIN_SPLIT) {
        aln += alignment;
        pad  = aln - data;
    }

    if (pad >= sizeof(heap_block) + MIN_SPLIT) {
        size_t prefix_data = pad - sizeof(heap_block);
        split(found, prefix_data);
        heap_block* aligned_blk = found->next;
        fl_remove(aligned_blk);

        found->is_free = true;
        fl_insert(found);

        split(aligned_blk, n);
        aligned_blk->is_free = false;
        allocated += aligned_blk->length;
        return block_to_ptr(aligned_blk);
    }

    split(found, n);
    found->is_free = false;
    allocated += found->length;
    return block_to_ptr(found);
}

void* realloc(void* ptr, size_t n) {
    if (!ptr) return malloc(n);
    if (!n)   { free(ptr); return nullptr; }

    n = align_up(n, MIN_ALIGN);
    heap_block* b       = ptr_to_block(ptr);
    size_t      old_len = b->length;

    if (old_len == n) return ptr;

    if (old_len > n) {
        split(b, n);
        allocated -= old_len - b->length;
        return ptr;
    }

    if (b->next && b->next->is_free &&
        b->length + sizeof(heap_block) + b->next->length >= n) {
        fl_remove(b->next);
        b->length += sizeof(heap_block) + b->next->length;
        b->next    = b->next->next;
        if (b->next) b->next->prev = b;
        split(b, n);
        allocated += b->length - old_len;
        return ptr;
    }

    void* p = malloc(n);
    if (!p) panic("heap::realloc: out of memory");
    memcpy(p, ptr, old_len);
    free(ptr);
    return p;
}

void* calloc(size_t n, size_t size) {
    void* p = malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}

void free(void* ptr) {
    if (!ptr) return;
    heap_block* b = ptr_to_block(ptr);
    if (b->is_free) panic("heap::free: double free");
    allocated -= b->length;
    b->is_free = true;
    b = coalesce(b);
    fl_insert(b);
}

}

extern "C" {
    void* exposed_malloc(size_t n) { return mem::heap::malloc(n); }
    void  exposed_free(void* p)    { mem::heap::free(p); }
}
