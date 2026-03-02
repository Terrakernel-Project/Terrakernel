#include "ps2m.hpp"
#include <arch/arch.hpp>
#include <cstdio>
#include <drivers/display/cursor/cursor.hpp>

extern int g_scr_width, g_scr_height;

#define PS2Leftbutton 0b00000001
#define PS2Middlebutton 0b00000100
#define PS2Rightbutton 0b00000010
#define PS2XSign 0b00010000
#define PS2YSign 0b00100000
#define PS2XOverflow 0b01000000
#define PS2YOverflow 0b10000000

void mouse_wait() {
    uint64_t timeout = 100000;
    while (timeout--) {
        if ((arch::x86_64::io::inb(0x64) & 0b10) == 0) {
            return;
        }
    }
}

void mouse_waitinput() {
    uint64_t timeout = 100000;
    while (timeout--) {
        if (arch::x86_64::io::inb(0x64) & 0b1) {
            return;
        }
    }
}

void mouse_write(uint8_t value) {
    mouse_wait();
    arch::x86_64::io::outb(0x64, 0xD4);
    mouse_wait();
    arch::x86_64::io::outb(0x60, value);
}

uint8_t mouse_read() {
    mouse_waitinput();
    return arch::x86_64::io::inb(0x60);
}

uint8_t mouse_cycle = 0;
uint8_t mouse_packet[4];
bool mouse_packet_ready = false;
struct {
    int x, y;
} mouse_pos, mouse_pos_old;

void process_mouse();

__attribute__((interrupt, hot))
void ps2m_interrupt_handler(void*) {
    uint8_t data = arch::x86_64::io::inb(0x60);

    static bool skip = true;
    if (skip) { skip = false; goto end; }
    switch(mouse_cycle) {
        case 0:
            if ((data & 0b00001000) == 0) {
            	printf("Misaligned byte\n\r");
            	break;
            }
            mouse_packet[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_packet_ready = true;
            mouse_cycle = 0;

            process_mouse();
            
            break;
    }

end:

    arch::x86_64::cpu::idt::send_eoi(12);
}

bool mLmb, mMmb, mRmb;

void process_mouse() {
    if (!mouse_packet_ready) return;

    bool xNegative, yNegative, xOverflow, yOverflow;

    if (mouse_packet[0] & PS2XSign) {
        xNegative = true;
    } else xNegative = false;

    if (mouse_packet[0] & PS2YSign) {
        yNegative = true;
    } else yNegative = false;

    if (mouse_packet[0] & PS2XOverflow) {
        xOverflow = true;
    } else xOverflow = false;

    if (mouse_packet[0] & PS2YOverflow) {
        yOverflow = true;
    } else yOverflow = false;

    if (!xNegative) {
        mouse_pos.x += mouse_packet[1];
        if (xOverflow) {
            mouse_pos.x += 255;
        }
    } else 
    {
        mouse_packet[1] = 256 - mouse_packet[1];
        mouse_pos.x -= mouse_packet[1];
        if (xOverflow) {
            mouse_pos.x -= 255;
        }
    }

    if (!yNegative) {
        mouse_pos.y -= mouse_packet[2];
        if (yOverflow) {
            mouse_pos.y -= 255;
        }
    } else {
        mouse_packet[2] = 256 - mouse_packet[2];
        mouse_pos.y += mouse_packet[2];
        if (yOverflow) {
            mouse_pos.y += 255;
        }
    }

    if (mouse_packet[0] & PS2Leftbutton) mLmb = true;
    else mLmb = false;
    
    if (mouse_packet[0] & PS2Middlebutton) mMmb = true;
    else mMmb = false;
    
    if (mouse_packet[0] & PS2Rightbutton) mRmb = true;
    else mRmb = false;
    

    if (mouse_pos.x < 0) mouse_pos.x = 0;
    if (mouse_pos.x > g_scr_width) mouse_pos.x = g_scr_width;
    
    if (mouse_pos.y < 0) mouse_pos.y = 0;
    if (mouse_pos.y > g_scr_height) mouse_pos.y = g_scr_height;
    
    drivers::display::cursor::render(mouse_pos.x, mouse_pos.y);

#ifdef CONFIG_DEBUG_MOUSE
	printf("Mouse input! %zu:%zu LMB=%B MMB=%B RMB=%B\n\r", mouse_pos.x, mouse_pos.y, mLmb, mMmb, mRmb);
#endif

    mouse_packet_ready = false;
    mouse_pos_old = mouse_pos;
}
            
namespace drivers::input::ps2m {

void initialise() {
    arch::x86_64::io::outb(0x64, 0xA8);

    while (arch::x86_64::io::inb(0x64) & 1)
        arch::x86_64::io::inb(0x60);

    arch::x86_64::cpu::idt::set_descriptor(0x2C, (uint64_t)ps2m_interrupt_handler, 0x8E);
    arch::x86_64::cpu::idt::irq_set_mask(2);
    arch::x86_64::cpu::idt::irq_set_mask(12);

    mouse_wait();
    arch::x86_64::io::outb(0x64, 0x20);
    mouse_waitinput();
    uint8_t status = arch::x86_64::io::inb(0x60);
    status |= 0b10;
    mouse_wait();
    arch::x86_64::io::outb(0x64, 0x60);
    mouse_wait();
    arch::x86_64::io::outb(0x60, status);

    mouse_write(0xFF);
    mouse_read();
    mouse_read();

    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF3);
    mouse_read();
    mouse_write(0xC8);
    mouse_read();
    mouse_write(0xE8);
    mouse_read();
    mouse_write(0x03);
    mouse_read();

    arch::x86_64::cpu::idt::send_eoi(12);
    arch::x86_64::cpu::idt::irq_clear_mask(2);
    arch::x86_64::cpu::idt::irq_clear_mask(12);

    mouse_write(0xF4);
    mouse_read();

    mouse_pos.x = g_scr_width / 2;
    mouse_pos.y = g_scr_height / 2;
}

MousePoint get_mouse_position() {
	return (MousePoint){mouse_pos.x, mouse_pos.y};
}

bool get_mouse_lmb_state() {
	return mLmb;
}

bool get_mouse_mmb_state() {
	return mMmb;
}

bool get_mouse_rmb_state() {
	return mRmb;
}

}
