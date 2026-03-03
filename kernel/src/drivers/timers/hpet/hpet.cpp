#include "hpet.hpp"
#include <cstdio>

#ifdef CONFIG_HPET_VERBOSE
#	define HDPRINTF(fmt, ...) printf("[ %s ] " fmt, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#else
#	define HDPRINTF(fmt, ...)
#endif

constexpr uint64_t REG_CAPABILITIES = 0x000;
constexpr uint64_t REG_CONFIGURATION = 0x010;
constexpr uint64_t REG_MAIN_COUNTER = 0x0F0;
constexpr uint64_t CFG_ENABLE = (1 << 0);

#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/types.h>
#include <mem/mem.hpp>
#include <cstdint>
#include <config.hpp>

struct address_structure {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
} __attribute__((packed));

struct hpet_table {
    struct {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oemid[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    } header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count : 5;
    uint8_t count_size_cap : 1;
    uint8_t reserved : 1;
    uint8_t legacy_irq_capable : 1;
    uint16_t pci_vendor_id;
    address_structure base_address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

namespace drivers::timers::hpet {

static uacpi_table hpet_handle;
static hpet_table* table = nullptr;
static bool ready = false;

static uint64_t base_address = 0;
static uint64_t femto_period = 0;
static uint32_t frequency_hz = 0;
static uint8_t num_timers = 0;

static uint64_t read_reg(uint64_t offset) {
    uint64_t val = *(volatile uint64_t*)(base_address + offset);
    HDPRINTF("read  [0x%03lx] -> 0x%016lx  (base=0x%016lx)\n\r", offset, val, base_address);
    return val;
}

static void write_reg(uint64_t offset, uint64_t value) {
    HDPRINTF("write [0x%03lx] <- 0x%016lx  (base=0x%016lx)\n\r", offset, value, base_address);
    *(volatile uint64_t*)(base_address + offset) = value;
}

uint64_t get_ticks() {
    if (!ready) {
        HDPRINTF("get_ticks called but HPET not ready! base_address=0x%016lx\n\r", base_address);
        return 0;
    }
    return read_reg(REG_MAIN_COUNTER);
}

uint64_t ns_elapsed_time() {
    if (!ready) {
        HDPRINTF("ns_elapsed_time called but HPET not ready!\n\r");
        return 0;
    }
    return (get_ticks() * femto_period) / 1000000;
}

void sleep_ms(uint64_t ms, bool called_by_apic) {
    if (!ready) {
        HDPRINTF("sleep_ms called but HPET not ready!\n\r");
        return;
    }

    if (called_by_apic)
        HDPRINTF("sleep requested by APIC, duration=%lums\n\r", ms);
    else
        HDPRINTF("sleep requested, duration=%lums\n\r", ms);

    uint64_t ticks = (ms * 1000000000000ULL) / femto_period;
    uint64_t start = get_ticks();
    HDPRINTF("sleeping for %lu ticks (start=%lu target=%lu)\n\r", ticks, start, start + ticks);

    while ((get_ticks() - start) < ticks)
        __asm__ volatile("pause");

    HDPRINTF("sleep done (end=%lu elapsed=%lu)\n\r", get_ticks(), get_ticks() - start);
}

void sleep_us(uint64_t us) {
    if (!ready) {
        HDPRINTF("sleep_us called but HPET not ready!\n\r");
        return;
    }

    HDPRINTF("sleep requested, duration=%luus\n\r", us);

    uint64_t ticks = (us * 1000000000ULL) / femto_period;
    uint64_t start = get_ticks();
    HDPRINTF("sleeping for %lu ticks (start=%lu target=%lu)\n\r", ticks, start, start + ticks);

    while ((get_ticks() - start) < ticks)
        __asm__ volatile("pause");

    HDPRINTF("sleep done (end=%lu elapsed=%lu)\n\r", get_ticks(), get_ticks() - start);
}

void initialise() {
    HDPRINTF("begin  ready=%d base_address=0x%016lx\n\r", (int)ready, base_address);
    HDPRINTF("searching for HPET ACPI table\n\r");

    uacpi_status status = uacpi_table_find_by_signature("HPET", &hpet_handle);
    if (uacpi_unlikely_error(status)) {
        Log::errf("HPET initialise: %s\n\r", uacpi_status_to_string(status));
        return;
    }

    HDPRINTF("HPET table found at %p\n\r", hpet_handle.ptr);

    table = (hpet_table*)hpet_handle.ptr;

    HDPRINTF("table dump:\n\r"
             "  hw_rev         = %u\n\r"
             "  comparators    = %u\n\r"
             "  64bit_counter  = %d\n\r"
             "  legacy_irq     = %d\n\r"
             "  pci_vendor     = 0x%04x\n\r"
             "  hpet_number    = %u\n\r"
             "  minimum_tick   = %u\n\r"
             "  addr_space_id  = %u\n\r"
             "  phys_base      = 0x%016lx\n\r",
             table->hardware_rev_id,
             table->comparator_count + 1,
             (int)table->count_size_cap,
             (int)table->legacy_irq_capable,
             table->pci_vendor_id,
             table->hpet_number,
             table->minimum_tick,
             table->base_address.address_space_id,
             table->base_address.address);

    if (table->base_address.address_space_id != 0) {
        Log::errf("HPET initialise: unsupported address space %u\n\r",
                  table->base_address.address_space_id);
        uacpi_table_unref(&hpet_handle);
        return;
    }

    uint64_t phys = table->base_address.address;
    base_address = (uint64_t)mem::vmm::pa_to_va(phys);
    num_timers = table->comparator_count + 1;

    HDPRINTF("mapping MMIO: phys=0x%016lx -> virt=0x%016lx\n\r", phys, base_address);
    mem::vmm::mmap((void*)phys, (void*)base_address, 1, PAGE_PRESENT | PAGE_RW | PAGE_PCD);
    HDPRINTF("MMIO mapped\n\r");

    uint64_t caps = read_reg(REG_CAPABILITIES);
    femto_period = caps >> 32;

    HDPRINTF("capabilities=0x%016lx  femto_period=%lu  rev=%lu  num_timers=%lu  64bit=%lu\n\r",
             caps, femto_period,
             caps & 0xFF,
             ((caps >> 8) & 0x1F) + 1,
             (caps >> 13) & 1);

    if (femto_period == 0 || femto_period > 0x05F5E100) {
        Log::errf("HPET initialise: invalid femto period %lu\n\r", femto_period);
        uacpi_table_unref(&hpet_handle);
        return;
    }

    frequency_hz = (uint32_t)(1000000000000000ULL / femto_period);
    HDPRINTF("frequency=%uHz\n\r", frequency_hz);

    uint64_t cfg = read_reg(REG_CONFIGURATION);
    HDPRINTF("config before init=0x%016lx  stopping counter\n\r", cfg);
    cfg &= ~CFG_ENABLE;
    write_reg(REG_CONFIGURATION, cfg);

    write_reg(REG_MAIN_COUNTER, 0);
    HDPRINTF("counter reset, current value=%lu\n\r", read_reg(REG_MAIN_COUNTER));

    cfg |= CFG_ENABLE;
    write_reg(REG_CONFIGURATION, cfg);
    HDPRINTF("counter started, config=0x%016lx  current tick=%lu\n\r", cfg, read_reg(REG_MAIN_COUNTER));

    ready = true;
    HDPRINTF("done  phys=0x%016lx virt=0x%016lx period=%lufs freq=%uHz timers=%u\n\r",
             phys, base_address, femto_period, frequency_hz, num_timers);
}

void disable() {
    if (!ready) {
        HDPRINTF("disable called but not ready, nothing to do\n\r");
        return;
    }

    HDPRINTF("disabling, current tick=%lu\n\r", get_ticks());

    uint64_t cfg = read_reg(REG_CONFIGURATION);
    cfg &= ~CFG_ENABLE;
    write_reg(REG_CONFIGURATION, cfg);

    HDPRINTF("counter stopped\n\r");

    ready = false;
    table = nullptr;
    base_address = 0;
    femto_period = 0;
    frequency_hz = 0;
    num_timers = 0;

    uacpi_table_unref(&hpet_handle);
    HDPRINTF("done\n\r");
}

}
