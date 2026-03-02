#include "apic.hpp"
#include <cstdio>

static volatile uint32_t* ioapic_base;

#define IOAPIC_REGSEL 0x00
#define IOAPIC_WIN    0x10

static inline void ioapic_write(uint32_t reg, uint32_t value) {
	if (!ioapic_base) return;
    *(volatile uint32_t*)((uintptr_t)ioapic_base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t*)((uintptr_t)ioapic_base + IOAPIC_WIN)    = value;
}

static inline uint32_t ioapic_read(uint32_t reg) {
	if (!ioapic_base) return (uint32_t)-1;
    *(volatile uint32_t*)((uintptr_t)ioapic_base + IOAPIC_REGSEL) = reg;
    return *(volatile uint32_t*)((uintptr_t)ioapic_base + IOAPIC_WIN);
}

namespace arch::x86_64::ioapic {

void initialise() {
	ioapic_base = (volatile uint32_t*)arch::x86_64::apic::get_ioapic_base();

	for (int i = 0; i < 64; i++) {
		ioapic_mask_irq(i);
		ioapic_register_interrupt(i, 0);
	}
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
    uint32_t gsi   = obtain_iso_gsi(irq);
    uint16_t flags = obtain_iso_flags(irq);

    uint32_t reg = 0x10 + gsi * 2;

    bool level_triggered = false;
    bool active_low      = false;

    if ((flags & 0b0011) == 0b0011)
        active_low = true;

    if ((flags & 0b1100) == 0b1100)
        level_triggered = true;

    uint32_t low =
        vector |
        (0 << 8) |
        (0 << 11) |
        (active_low ? (1 << 13) : 0) |
        (level_triggered ? (1 << 15) : 0) |
        (1 << 16);

    uint32_t high =
        get_bsp_apic_id_u32() << 24;

    ioapic_write(reg,     low);
    ioapic_write(reg + 1, high);

    low &= ~(1 << 16);
    ioapic_write(reg, low);
}

void ioapic_list_interrupts() {
	for (int i = 0; i < 64; i++) {
		uint32_t reg_a = 0x10 + i * 2;
		uint32_t low_a = ioapic_read(reg_a);

		bool reg_a_valid = (i <= (uint8_t)-1);

		i++;

		uint32_t reg_b = 0x10 + (i+1) * 2;
		uint32_t low_b = ioapic_read(reg_b);

		bool reg_b_valid = (i <= (uint8_t)-1);

		i++;

		uint32_t reg_c = 0x10 + (i+2) * 2;
		uint32_t low_c = ioapic_read(reg_c);

		bool reg_c_valid = (i <= (uint8_t)-1);

		i++;

		uint32_t reg_d = 0x10 + (i+3) * 2;
		uint32_t low_d = ioapic_read(reg_d);

		bool reg_d_valid = (i <= (uint8_t)-1);

		i++;

		uint32_t reg_e = 0x10 + (i+4) * 2;
		uint32_t low_e = ioapic_read(reg_e);

		bool reg_e_valid = (i <= (uint8_t)-1);

		i++;

		uint32_t reg_f = 0x10 + (i+5) * 2;
		uint32_t low_f = ioapic_read(reg_f);

		bool reg_f_valid = (i <= (uint8_t)-1);

		low_a &= 0xFF;
		low_b &= 0xFF;
		low_c &= 0xFF;
		low_d &= 0xFF;
		low_e &= 0xFF;
		low_f &= 0xFF;
		
		if (reg_a_valid) printf("IRQ %03u: %03u | ",    i-5, low_a);
		if (reg_b_valid) printf("IRQ %03u: %03u | ",    i-4, low_b);
		if (reg_c_valid) printf("IRQ %03u: %03u | ",    i-3, low_c);
		if (reg_d_valid) printf("IRQ %03u: %03u | ",    i-2, low_d);
		if (reg_e_valid) printf("IRQ %03u: %03u | ",    i-1, low_e);
		if (reg_f_valid) printf("IRQ %03u: %03u \n\r",  i, low_f);
	}
}

}