#include <uacpi/kernel_api.h>
#include <limine.h>
#include <mem/mem.hpp>
#include <cstdio>
#include <cstdarg>
#include <arch/arch.hpp>
#include <drivers/timers/apic/apic.hpp>
#include <panic.hpp>
#include <cstdint>
#include <proc/spinlocks.hpp>
#include "kernel_api.hpp"

extern "C" {

__attribute__((section(".limine_requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    if (!rsdp_request.response || !rsdp_request.response->address) panic((char*)"K_NO_RSDP");
    *out_rsdp_address = reinterpret_cast<uacpi_phys_addr>(rsdp_request.response->address);
    return UACPI_STATUS_OK;
}

#define PAGE_FLOOR(addr) ((addr) & ~(0x1000 - 1ULL))

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    void* base = (void*)PAGE_FLOOR((uint64_t)addr);
    uint64_t pages = (len + ((uint64_t)addr & 0xFFF) + 0xFFF) / 0x1000;

    mem::vmm::mmap((void*)base, (void*)base, pages, PAGE_PRESENT | PAGE_RW);

    return (void*)((uint64_t)base + (addr & 0xFFF));
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    void* base = (void*)PAGE_FLOOR((uint64_t)addr);
    uint64_t pages = (len + ((uint64_t)addr & 0xFFF) + 0xFFF) / 0x1000;
    
    mem::vmm::munmap(base, pages);
}

#include <config.hpp>
#ifndef UACPI_FORMATTED_LOGGING
void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* s) {
    (void)lvl;
#ifdef CONFIG_ACPI_VERBOSE
    print_time();
    printf("[ \x1b[95mUACPI\x1b[0m ] %s", s);
#endif
}
#else
void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* s, ...) {
#ifdef CONFIG_ACPI_VERBOSE
    print_time();
    printf("[ \x1b[95mUACPI\x1b[0m ] ");
    va_list va;
    va_start(va, s);
    vprintf(s, va);
    va_end(va);
#endif
}

void uacpi_kernel_vlog(uacpi_log_level lvl, const uacpi_char* s, uacpi_va_list va) {
#ifdef CONFIG_ACPI_VERBOSE
    print_time();
    printf("[ \x1b[95mUACPI\x1b[0m ] ");
    vprintf(s, va);
#endif
}
#endif

uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl) {
    (void)current_init_lvl;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_deinitialize(void) {}

#define MAX_PCI_DEVICES 512
struct pci_dev {
    uint8_t b, d, f;
    bool in_use;
};

static pci_dev pci_devices[MAX_PCI_DEVICES];

pci_dev* pci_get_free_slot() {
    for (int i = 0; i < MAX_PCI_DEVICES; i++) {
        if (!pci_devices[i].in_use) {
            pci_devices[i].in_use = true;
            return &pci_devices[i];
        }
    }
    return nullptr;
}

void pci_close_slot(pci_dev* dev) {
    if (dev >= pci_devices && dev < pci_devices + MAX_PCI_DEVICES) {
        dev->in_use = false;
    }
}

uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle
) {
    pci_dev* dev = pci_get_free_slot();
    if (!dev) return UACPI_STATUS_OUT_OF_MEMORY;

    dev->b = address.bus;
    dev->d = address.device;
    dev->f = address.function;
    
    *out_handle = dev;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
    pci_close_slot((pci_dev*)handle);
}

uint32_t uacpi_pci_read_helper(int sz_order, uint8_t bus, uint8_t device, uint8_t func, uacpi_size offset) {
    using namespace arch::x86_64::io;

    uint32_t address = 0x80000000 | ((bus & 0xFF) << 16) | ((device & 0x1F) << 11) |
                       ((func & 0x07) << 8) | (offset & 0xFC);

    outl(0xCF8, address);
    uint32_t data = inl(0xCFC);

    switch (sz_order) {
        case 0:
            return (data >> ((offset & 3) * 8)) & 0xFF;
        case 1:
            return (data >> ((offset & 2) * 8)) & 0xFFFF;
        case 2:
            return data;
        default:
            return 0;
    }
}

void uacpi_pci_write_helper(int sz_order, uint8_t bus, uint8_t device, uint8_t func, uacpi_size offset, uint32_t value) {
    using namespace arch::x86_64::io;

    uint32_t address = 0x80000000 | ((bus & 0xFF) << 16) | ((device & 0x1F) << 11) |
                       ((func & 0x07) << 8) | (offset & 0xFC);

    outl(0xCF8, address);
    uint32_t data = inl(0xCFC);

    switch (sz_order) {
        case 0:
            data &= ~(0xFF << ((offset & 3) * 8));
            data |= (value & 0xFF) << ((offset & 3) * 8);
            break;
        case 1:
            data &= ~(0xFFFF << ((offset & 2) * 8));
            data |= (value & 0xFFFF) << ((offset & 2) * 8);
            break;
        case 2:
            data = value;
            break;
        default:
            return;
    }

    outl(0xCFC, data);
}

uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    pci_dev* dev = (pci_dev*)device;
    *value = uacpi_pci_read_helper(0, dev->b, dev->d, dev->f, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    pci_dev* dev = (pci_dev*)device;
    *value = uacpi_pci_read_helper(1, dev->b, dev->d, dev->f, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    pci_dev* dev = (pci_dev*)device;
    *value = uacpi_pci_read_helper(2, dev->b, dev->d, dev->f, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    pci_dev* dev = (pci_dev*)device;
    uacpi_pci_write_helper(0, dev->b, dev->d, dev->f, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    pci_dev* dev = (pci_dev*)device;
    uacpi_pci_write_helper(1, dev->b, dev->d, dev->f, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    pci_dev* dev = (pci_dev*)device;
    uacpi_pci_write_helper(2, dev->b, dev->d, dev->f, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    (void)len;
    *out_handle = (uacpi_handle)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    (void)handle;
}

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value
) {
    *out_value = arch::x86_64::io::inb((uint16_t)((uintptr_t)handle + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value
) {
    *out_value = arch::x86_64::io::inw((uint16_t)((uintptr_t)handle + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value
) {
    *out_value = arch::x86_64::io::inl((uint16_t)((uintptr_t)handle + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value
) {
    arch::x86_64::io::outb((uint16_t)((uintptr_t)handle + offset), in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value
) {
    arch::x86_64::io::outw((uint16_t)((uintptr_t)handle + offset), in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value
) {
    arch::x86_64::io::outl((uint16_t)((uintptr_t)handle + offset), in_value);
    return UACPI_STATUS_OK;
}

void* uacpi_kernel_alloc(uacpi_size size) {
    return mem::heap::malloc(size);
}

#ifdef UACPI_NATIVE_ALLOC_ZEROED
void* uacpi_kernel_alloc_zeroed(uacpi_size size) {
    return mem::heap::calloc(1, size);
}
#endif

#ifndef UACPI_SIZED_FREES
void uacpi_kernel_free(void* mem) {
    mem::heap::free(mem);
}
#else
void uacpi_kernel_free(void* mem, uacpi_size size_hint) {
    (void)size_hint;
    mem::heap::free(mem);
}
#endif

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return drivers::timers::apic::ns_elapsed_time();
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    if (usec == 0) return;
    
    uint64_t start = uacpi_kernel_get_nanoseconds_since_boot();
    uint64_t end = start + ((uint64_t)usec * 1000ULL);

    while (uacpi_kernel_get_nanoseconds_since_boot() < end) {
        __asm__ __volatile__("pause");
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    uint64_t duration_ns = msec * 1000000ULL;
    uint64_t start = uacpi_kernel_get_nanoseconds_since_boot();
    uint64_t end = start + duration_ns;

    while (uacpi_kernel_get_nanoseconds_since_boot() < end) {
        __asm__ __volatile__("pause");
    }
}

struct mutex {
    volatile bool locked;
};

uacpi_handle uacpi_kernel_create_mutex(void) {
    mutex* m = (mutex*)mem::heap::malloc(sizeof(mutex));
    if (!m) return nullptr;
    m->locked = false;
    return m;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    mem::heap::free(handle);
}

struct event {
    volatile bool signaled;
};

uacpi_handle uacpi_kernel_create_event(void) {
    event* e = (event*)mem::heap::malloc(sizeof(event));
    if (!e) return nullptr;
    e->signaled = false;
    return e;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    mem::heap::free(handle);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return (uacpi_thread_id)1;
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout) {
    mutex* m = (mutex*)handle;
    if (!m) return UACPI_STATUS_NOT_FOUND;

    if (timeout == 0x0000) {
        if (!m->locked) {
            m->locked = true;
            return UACPI_STATUS_OK;
        }
        return UACPI_STATUS_TIMEOUT;
    }

    if (timeout == 0xFFFF) {
        while (m->locked) {
            __asm__ __volatile__("pause");
        }
        m->locked = true;
        return UACPI_STATUS_OK;
    }

    uint64_t start_ns = uacpi_kernel_get_nanoseconds_since_boot();
    uint64_t timeout_ns = (uint64_t)timeout * 1000000ULL;

    while (m->locked) {
        uint64_t now_ns = uacpi_kernel_get_nanoseconds_since_boot();
        if (now_ns - start_ns >= timeout_ns) {
            return UACPI_STATUS_TIMEOUT;
        }
        __asm__ __volatile__("pause");
    }

    m->locked = true;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    mutex* m = (mutex*)handle;
    if (m) m->locked = false;
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
    event* e = (event*)handle;
    if (!e) return false;

    if (timeout == 0x0000) {
        return e->signaled;
    }

    if (timeout == 0xFFFF) {
        while (!e->signaled) {
            __asm__ __volatile__("pause");
        }
        return true;
    }

    uint64_t start_ns = uacpi_kernel_get_nanoseconds_since_boot();
    uint64_t timeout_ns = (uint64_t)timeout * 1000000ULL;

    while (!e->signaled) {
        uint64_t now_ns = uacpi_kernel_get_nanoseconds_since_boot();
        if (now_ns - start_ns >= timeout_ns) {
            return false;
        }
        __asm__ __volatile__("pause");
    }

    return true;
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
    event* e = (event*)handle;
    if (e) e->signaled = true;
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
    event* e = (event*)handle;
    if (e) e->signaled = false;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* request) {
    if (request->type == UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT) {
    } else if (request->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL) {
        panic((char*)"UACPI_FIRMWARE_REQUEST_TYPE_FATAL");
    }
    return UACPI_STATUS_OK;
}

#define MAX_CACHED_INTERRUPTS 32

struct interrupt {
    uint8_t vector;
    uint8_t irq;
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    bool active;
};

static interrupt cached_interrupts[MAX_CACHED_INTERRUPTS];

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    uint8_t vector = irq + 0x20;

    interrupt* i = nullptr;
    int idx = 0;
    for (; idx < MAX_CACHED_INTERRUPTS; idx++) {
        if (!cached_interrupts[idx].active) {
            i = &cached_interrupts[idx];
            break;
        }
    }
    
    if (!i) return UACPI_STATUS_OUT_OF_MEMORY;
    
    i->vector = vector;
    i->irq = irq;
    i->handler = handler;
    i->ctx = ctx;
    i->active = true;

    *out_irq_handle = i;

    arch::x86_64::cpu::idt::set_descriptor(vector, (uint64_t)handler, 0x8E);

#ifdef CONFIG_ACPI_VERBOSE
    Log::infof("Loaded and cached interrupt: cache_id=%d vector=0x%02X handle=0x%016X attr=0x%02X", idx, i->vector, (uint64_t)i->handler, 0x8E);
#endif

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler unused, uacpi_handle irq_handle
) {
    (void)unused;
    interrupt* i = (interrupt*)irq_handle;
    if (i && i->active) {
        arch::x86_64::cpu::idt::clear_descriptor(i->vector);
        i->active = false;
    }
    return UACPI_STATUS_OK;
}

void acpi_reload_interrupts() {
    for (int idx = 0; idx < MAX_CACHED_INTERRUPTS; idx++) {
        if (cached_interrupts[idx].active) {
            interrupt* i = &cached_interrupts[idx];
            arch::x86_64::cpu::idt::set_descriptor(i->vector, (uint64_t)i->handler, 0x8E);
#ifdef CONFIG_ACPI_VERBOSE
            Log::infof("Reloaded interrupt: cache_id=%d vector=0x%02X handle=0x%016X attr=0x%02X", idx, i->vector, (uint64_t)i->handler, 0x8E);
#endif
        }
    }
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    return (uacpi_handle)new_spinlock("uacpi");
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    delete_spinlock((spinlock*)handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    acquire_spinlock((spinlock*)handle);
    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    (void)flags;
    release_spinlock((spinlock*)handle);
}

#define MAX_WORK 32

struct work {
    uacpi_work_type type;
    uacpi_work_handler handler;
    uacpi_handle ctx;
    bool active;
};

static work work_queue[MAX_WORK];

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx
) {
    for (int i = 0; i < MAX_WORK; i++) {
        if (!work_queue[i].active) {
            work_queue[i].type = type;
            work_queue[i].handler = handler;
            work_queue[i].ctx = ctx;
            work_queue[i].active = true;

            handler(ctx);
            
            work_queue[i].active = false;
            return UACPI_STATUS_OK;
        }
    }
    return UACPI_STATUS_OUT_OF_MEMORY;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}

}
