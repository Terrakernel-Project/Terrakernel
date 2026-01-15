#include "spinlocks.h"

static inline uint8_t atomic_test_and_set(volatile uint8_t* ptr) {
    uint8_t old;
    asm volatile(
        "lock xchg %0, %1"
        : "=r"(old), "+m"(*ptr)
        : "0"(1)
        : "memory"
    );
    return old;
}

void c_acquire_spinlock(struct spinlock* lock, int timeout) {
    int timeout_counter = 0;
    while (atomic_test_and_set(&lock->locked)) {
        asm volatile("pause");
        timeout_counter++;
        if (timeout_counter >= timeout) {
            break;
        }
    }
}

void c_release_spinlock(struct spinlock* lock) {
    asm volatile("" ::: "memory");
    lock->locked = 0;
}
