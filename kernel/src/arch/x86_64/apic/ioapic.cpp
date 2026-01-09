#include "apic.hpp"
#include <cstdio>

static volatile uint32_t* ioapic_base;

static inline void ioapic_write(uint32_t reg, uint32_t value) {
    ioapic_base[0] = reg;
    ioapic_base[4] = value;
}

static inline uint32_t ioapic_read(uint32_t reg) {
    ioapic_base[0] = reg;
    return ioapic_base[4];
}

namespace arch::x86_64::ioapic {

void initialise() {
	ioapic_base = (volatile uint32_t*)arch::x86_64::apic::get_ioapic_base();
}

void ioapic_mask_irq(uint8_t irq) {
    uint32_t reg = 0x10 + irq * 2;
    uint32_t low = ioapic_read(reg);
    low |= (1 << 16);
    ioapic_write(reg, low);
}

void ioapic_unmask_irq(uint8_t irq) {
    uint32_t reg = 0x10 + irq * 2;
    uint32_t low = ioapic_read(reg);
    low &= ~(1 << 16);
    ioapic_write(reg, low);
}

void ioapic_register_interrupt(uint8_t irq, uint8_t vector) {
    uint32_t reg = 0x10 + irq * 2;
    uint32_t low = ioapic_read(reg);
    low &= ~0xFF;
    low |= vector;
    ioapic_write(reg, low);
}


}
