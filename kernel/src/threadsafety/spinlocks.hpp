#ifndef SPINLOCK_HPP
#define SPINLOCK_HPP 1

#include <cstdint>
#include "atomic.hpp"

class Spinlock {
private:
    volatile uint8_t lock;
    const char* name;
    uint32_t holder_cpu;
    
public:
    explicit Spinlock(const char* lock_name = "???") 
        : lock(0), name(lock_name), holder_cpu(0xFFFFFFFF) {}
    
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;
    
    void acquire() {
        while (__sync_lock_test_and_set(&lock, 1)) {
            while (lock) {
                asm volatile ("pause" ::: "memory");
            }
        }
    }

    void acquire(uint64_t id) {
    	acquire();
    	holder_cpu = id;
    }
    
    void release() {
        holder_cpu = 0xFFFFFFFF;
        __sync_lock_release(&lock);
    }
    
    bool try_acquire() {
        if (__sync_lock_test_and_set(&lock, 1) == 0) {
            return true;
        }
        return false;
    }
    
    bool is_locked() const {
        return __atomic_load_n(&lock, __ATOMIC_RELAXED) != 0;
    }
    
    const char* get_name() const {
        return name;
    }
    
    uint32_t get_holder() const {
        return holder_cpu;
    }
};

#endif
