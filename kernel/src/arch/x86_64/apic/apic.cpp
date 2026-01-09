#include "apic.hpp"
#include <arch/arch.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/types.h>
#include <cstdio>
#include <mem/mem.hpp>
#include <exec/elf.hpp>
#include <drivers/timers/pit/pit.hpp>
#include <drivers/timers/apic/apic.hpp>
#include <config.hpp>

#define MADT_ENTRY_LAPIC    0x0
#define MADT_ENTRY_IOAPIC   0x1
#define MADT_ENTRY_ISO      0x2
#define MADT_ENTRY_NMI      0x4

#define IA32_APIC_BASE_MSR      0x1B
#define IA32_APIC_BASE_BSP      (1 << 8)
#define IA32_X2APIC_ENABLE      (1 << 10)
#define IA32_APIC_GLOBAL_ENABLE (1 << 11)

#define APIC_REG_ID             0x020
#define APIC_REG_VERSION        0x030
#define APIC_REG_TPR            0x080
#define APIC_REG_EOI            0x0B0
#define APIC_REG_SPURIOUS       0x0F0
#define APIC_REG_ICR_LOW        0x300
#define APIC_REG_ICR_HIGH       0x310
#define APIC_REG_LVT_TIMER      0x320
#define APIC_REG_LVT_THERMAL    0x330
#define APIC_REG_LVT_PERF       0x340
#define APIC_REG_LVT_LINT0      0x350
#define APIC_REG_LVT_LINT1      0x360
#define APIC_REG_LVT_ERROR      0x370
#define APIC_REG_TIMER_INITIAL  0x380
#define APIC_REG_TIMER_CURRENT  0x390
#define APIC_REG_TIMER_DIVIDE   0x3E0

#define IPI_VECTOR 		0xF0
#define TIMER_VECTOR 	0xF1

#define APIC_LVT_INT_MASKED 		0x10000
#define APIC_LVT_TIMER_MODE_PERIODIC 0x20000

struct madt_lapic_entry {
    uint8_t type;
    uint8_t length;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct madt_ioapic_entry {
    uint8_t type;
    uint8_t length;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed));

struct acpi_sdt_hdr {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct madt_table {
    acpi_sdt_hdr header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed));

namespace arch::x86_64::apic {

static uacpi_table madt_handle;
static madt_table* madt = nullptr;
static volatile uint32_t* lapic_base = nullptr;
volatile uint32_t* ioapic_base = nullptr;
static cpu_registry* registry = nullptr;
static cpu_unit* tail_unit = nullptr;
static bool x2apic_enabled = false;
static bool enabled = false;
static cpu_unit* bsp;

static bool is_bsp(uint32_t apic_id) {
	if (!bsp) return true;
    return bsp->apic_id == apic_id;
}

static bool check_x2apic_support() {
    bool is_enabled = (arch::x86_64::misc::cpuid(1, 0).ecx & CPUID_FEAT_X2APIC) != 0;
    if (!is_enabled) {
        mem::vmm::mmap((void*)lapic_base, (void*)lapic_base, 1, PAGE_PRESENT | PAGE_RW);
        mem::vmm::mmap((void*)ioapic_base, (void*)ioapic_base, 1, PAGE_PRESENT | PAGE_RW);
    }
    return is_enabled;
}

static void enable_x2apic() {
    uint64_t apic_base_msr = arch::x86_64::misc::rdmsr(IA32_APIC_BASE_MSR);
    apic_base_msr |= IA32_X2APIC_ENABLE | IA32_APIC_GLOBAL_ENABLE;
    arch::x86_64::misc::wrmsr(IA32_APIC_BASE_MSR, apic_base_msr);
    x2apic_enabled = true;
}

uint32_t apic_read_reg(cpu_unit* cpu, uint32_t reg) {
    if (cpu) {
        if (cpu->x2apic_enabled) {
            uint32_t msr = 0x800 + (reg >> 4);
            return (uint32_t)arch::x86_64::misc::rdmsr(msr);
        } else {
            return ((volatile uint8_t*)cpu->lapic_base)[reg >> 2];
        }
    } else {
        return ((volatile uint8_t*)lapic_base)[reg >> 2];
    }
}

void apic_write_reg(cpu_unit* cpu, uint32_t reg, uint32_t value) {
    if (cpu) {
        if (cpu->x2apic_enabled) {
            uint32_t msr = 0x800 + (reg >> 4);
            arch::x86_64::misc::wrmsr(msr, value);
        } else {
            ((volatile uint8_t*)cpu->lapic_base)[reg >> 2] = value;
        }
    } else {
        ((volatile uint8_t*)lapic_base)[reg >> 2] = value;
    }
}

static void register_new_cpu_unit(madt_lapic_entry* lapic, uint32_t index) {
    cpu_unit* new_unit = (cpu_unit*)mem::heap::malloc(sizeof(cpu_unit));
    
    new_unit->registry_id = index;
    new_unit->apic_id = lapic->apic_id;
    new_unit->x2apic_enabled = x2apic_enabled;
    new_unit->is_bsp = is_bsp(lapic->apic_id);
    new_unit->online = false;
    new_unit->lapic_base = (void*)lapic_base;
    new_unit->kernel_stack = stack_manager_get_new_stack(2, false);
    new_unit->interrupt_stack = stack_manager_get_new_stack(2, false);
    new_unit->current_thread_id = 0;
    new_unit->gdt_base = get_gdt_base();
    new_unit->idt_base = get_idt_base();
    new_unit->tss_base = get_tss_base();
    new_unit->next_unit = nullptr;

    new_unit->read_reg = apic_read_reg;
    new_unit->write_reg = apic_write_reg;
    
    if (registry->first_unit == nullptr) {
        registry->first_unit = new_unit;
        tail_unit = new_unit;
    } else {
        tail_unit->next_unit = new_unit;
        tail_unit = new_unit;
    }
    
    if (new_unit->is_bsp) {
        new_unit->online = true;
        registry->bsp_index = index;
#ifdef APIC_VERBOSE
        printf("CPU %d (APIC ID %d) is BSP\n", index, new_unit->apic_id);
#endif
    }

    if (!bsp) bsp = new_unit;
}

static void parse_madt_entries() {
    uint8_t* ptr = madt->entries;
    uint8_t* end = (uint8_t*)madt + madt->header.length;
    uint32_t cpu_index = 0;
    
    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];
        
        switch (type) {
            case MADT_ENTRY_LAPIC: {
                madt_lapic_entry* lapic = (madt_lapic_entry*)ptr;
#ifdef APIC_VERBOSE
                printf("Local APIC: ACPI ID=%02X, APIC ID=%02X\n",
                       lapic->acpi_processor_id, lapic->apic_id);
#endif
                register_new_cpu_unit(lapic, cpu_index);
                cpu_index++;
                break;
            }
            case MADT_ENTRY_IOAPIC: {
                madt_ioapic_entry* ioapic = (madt_ioapic_entry*)ptr;
                ioapic_base = (volatile uint32_t*)(uintptr_t)ioapic->ioapic_addr;
#ifdef APIC_VERBOSE
                printf("I/O APIC: ID=%02X, Address=0x%08X\n",
                       ioapic->ioapic_id, ioapic->ioapic_addr);
#endif
                break;
            }
        }
        
        ptr += length;
    }
    
    registry->num_units = cpu_index;
}

__attribute__((interrupt))
void ipi_handle(void*);

void initialise() {
	if (arch::x86_64::misc::cpuid(1, 0).eax & CPUID_FEAT_APIC) {} else return;

    uacpi_status status = uacpi_table_find_by_signature("APIC", &madt_handle);
    if (uacpi_unlikely_error(status)) {
        Log::errf("APIC initialise: %s\n", uacpi_status_to_string(status));
        return;
    }
    
    madt = (madt_table*)madt_handle.ptr;
    lapic_base = (volatile uint32_t*)(uintptr_t)madt->local_apic_address;
    
    bool x2apic_supported = check_x2apic_support();
#ifdef APIC_VERBOSE
    printf("Got an %sAPIC\n", x2apic_supported ? "x2" : "x");
#endif
    
    if (x2apic_supported) {
        enable_x2apic();
#ifdef APIC_VERBOSE
		printf("x2APIC enabled\n");
#endif
    }
    
    registry = (cpu_registry*)mem::heap::malloc(sizeof(cpu_registry));
    registry->first_unit = nullptr;
    registry->num_units = 0;
    registry->bsp_index = 0;
    
    parse_madt_entries();

#ifdef APIC_VERBOSE    
    printf("Found %d CPUs\n", registry->num_units);
#endif

    enabled = true;

	arch::x86_64::cpu::idt::set_descriptor(IPI_VECTOR, (uint64_t)ipi_handle, 0x8E);
}

void cleanup() {
    cpu_unit* curr = registry->first_unit;
    while (curr != nullptr) {
        cpu_unit* next = curr->next_unit;
        mem::heap::free(curr);
        curr = next;
    }
    mem::heap::free(registry);
    uacpi_table_unref(&madt_handle);
}

cpu_unit* get_cpu(uint32_t index) {
    cpu_unit* curr = registry->first_unit;
    while (curr != nullptr) {
        if (curr->registry_id == index) {
            return curr;
        }
        curr = curr->next_unit;
    }
    return nullptr;
}

cpu_unit* get_bsp() {
    return get_cpu(registry->bsp_index);
}

static uint32_t get_local_apic_id() {
    if (x2apic_enabled) {
        return (uint32_t)arch::x86_64::misc::rdmsr(0x802);
    } else {
        uint32_t val = apic_read_reg(nullptr, APIC_REG_ID);
        return (val >> 24) & 0xFF;
    }
}

cpu_unit* get_current_cpu() {
    uint32_t apic_id = get_local_apic_id();

    cpu_unit* curr = registry->first_unit;
    while (curr != nullptr) {
        if (curr->apic_id == apic_id) {
            return curr;
        }
        curr = curr->next_unit;
    }
#ifdef APIC_VERBOSE
    printf("Didn't find a CPU with APIC ID %u\n", apic_id);
#endif
    return nullptr;
}

void* get_ioapic_base() {
	return (void*)ioapic_base;
}

cpu_registry* get_cpu_registry() {
	return registry;
}

void wake_up_cpus() {
	
}

void ipi_send(cpu_unit* unit, uint8_t vector) {
    if (!unit) return;

    while (unit->read_reg(unit, APIC_REG_ICR_LOW) & (1 << 12))
        ;

    unit->write_reg(unit, APIC_REG_ICR_HIGH, unit->apic_id << 24);

    unit->write_reg(
    	unit,
        APIC_REG_ICR_LOW,
        vector |
        (0 << 8) |
        (0 << 11) |
        (1 << 14)
    );
}

__attribute__((interrupt))
void ipi_handle(void*) {
#ifndef APIC_VERBOSE
	printf("got an IPI\n\r");
#endif

    arch::x86_64::cpu::idt::send_eoi(0);
}

}

bool apic_enabled() {
	return arch::x86_64::apic::enabled;
}

void apic_send_eoi() {
	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_EOI, 0);
}

namespace drivers::timers::apic {

uint64_t calibrate_pit() {
	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_TIMER_INITIAL, 0xFFFFFFFF);
	drivers::timers::pit::sleep_ms(1);
	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_LVT_TIMER, APIC_LVT_INT_MASKED);

	uint32_t ticks_in_1ms = 0xFFFFFFFF - arch::x86_64::apic::bsp->read_reg(arch::x86_64::apic::bsp, APIC_REG_TIMER_CURRENT);

	drivers::timers::pit::disable();

	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_LVT_TIMER, TIMER_VECTOR | APIC_LVT_TIMER_MODE_PERIODIC);
	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_TIMER_DIVIDE, 0x3);
	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_TIMER_INITIAL, ticks_in_1ms);

#ifdef APIC_VERBOSE
	printf("Took %zu ticks for 1 milliseconds...\n\r", ticks_in_1ms);
#endif

	return ticks_in_1ms;
}

void initialise() {
	initialise_timer();

	arch::x86_64::apic::bsp->write_reg(arch::x86_64::apic::bsp, APIC_REG_TIMER_DIVIDE, 0x3);

	give_timer_ticks(calibrate_pit());
}

}
