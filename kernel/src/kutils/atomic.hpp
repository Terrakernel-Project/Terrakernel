#ifndef ATOMIC_HPP
#define ATOMIC_HPP

#include <cstddef>

/*
    define them using #define so that the preprocessor shuts up
*/

#define __ATOMIC_RELAXED   0
#define __ATOMIC_CONSUME   1
#define __ATOMIC_ACQUIRE   2
#define __ATOMIC_RELEASE   3
#define __ATOMIC_ACQ_REL   4
#define __ATOMIC_SEQ_CST   5

#define __atomic_load_n        __atomic_load_n
#define __atomic_store_n       __atomic_store_n
#define __atomic_exchange_n    __atomic_exchange_n
#define __atomic_compare_exchange_n __atomic_compare_exchange_n
#define __atomic_fetch_add     __atomic_fetch_add
#define __atomic_fetch_sub     __atomic_fetch_sub
#define __atomic_fetch_and     __atomic_fetch_and
#define __atomic_fetch_or      __atomic_fetch_or
#define __atomic_fetch_xor     __atomic_fetch_xor
#define __atomic_fetch_nand    __atomic_fetch_nand
#define __atomic_add_fetch     __atomic_add_fetch
#define __atomic_sub_fetch     __atomic_sub_fetch
#define __atomic_and_fetch     __atomic_and_fetch
#define __atomic_or_fetch      __atomic_or_fetch
#define __atomic_xor_fetch     __atomic_xor_fetch
#define __atomic_nand_fetch    __atomic_nand_fetch

#define __atomic_test_and_set  __atomic_test_and_set
#define __atomic_clear         __atomic_clear

#define __atomic_thread_fence  __atomic_thread_fence
#define __atomic_signal_fence  __atomic_signal_fence
#define __atomic_is_lock_free  __atomic_is_lock_free

#define __sync_fetch_and_add   __sync_fetch_and_add
#define __sync_fetch_and_sub   __sync_fetch_and_sub
#define __sync_fetch_and_or    __sync_fetch_and_or
#define __sync_fetch_and_and   __sync_fetch_and_and
#define __sync_fetch_and_xor   __sync_fetch_and_xor
#define __sync_fetch_and_nand  __sync_fetch_and_nand

#define __sync_add_and_fetch   __sync_add_and_fetch
#define __sync_sub_and_fetch   __sync_sub_and_fetch
#define __sync_or_and_fetch    __sync_or_and_fetch
#define __sync_and_and_fetch   __sync_and_and_fetch
#define __sync_xor_and_fetch   __sync_xor_and_fetch
#define __sync_nand_and_fetch  __sync_nand_and_fetch

#define __sync_bool_compare_and_swap __sync_bool_compare_and_swap
#define __sync_val_compare_and_swap  __sync_val_compare_and_swap

#define __sync_lock_test_and_set __sync_lock_test_and_set
#define __sync_lock_release      __sync_lock_release
#define __sync_synchronize       __sync_synchronize

#endif
