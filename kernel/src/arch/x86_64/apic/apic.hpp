#ifndef APIC_HPP
#define APIC_HPP 1

#include <cstdint>

struct cpu_unit {
	uint32_t registry_id;

    uint32_t apic_id;
    bool x2apic_enabled;
    bool is_bsp;
    bool online;
    
    void* lapic_base;
    
    void (*write_reg)(cpu_unit*,uint32_t, uint32_t);
    uint32_t (*read_reg)(cpu_unit*,uint32_t);
    
    void* kernel_stack;
    void* interrupt_stack;
    
    uint32_t current_thread_id;
    
    void* gdt_base;
    void* idt_base;
	void* tss_base;

    cpu_unit* next_unit;
};

struct cpu_registry {
	cpu_unit* first_unit;
	uint32_t num_units;
	uint32_t bsp_index;
};

namespace arch::x86_64::apic {

void initialise();
cpu_registry* get_cpu_registry();

cpu_unit* get_cpu(uint32_t index);
cpu_unit* get_current_cpu();
cpu_unit* get_bsp();

void* get_ioapic_base();

}

#include "../cpu/idt.hpp"

namespace arch::x86_64::ioapic {

void initialise();

void ioapic_mask_irq(uint8_t irq);
void ioapic_unmask_irq(uint8_t irq);
void ioapic_register_interrupt(uint8_t irq, uint8_t vector);

}

bool apic_enabled();
void apic_send_eoi();

void init_cores();

namespace drivers::timers::apic {

void initialise();

}

#endif
