#include "rtl8139.hpp"
#include <cstdio>
#include <arch/arch.hpp>
#include <mem/mem.hpp>
#include <panic.hpp>

using namespace arch::x86_64::io;

#define MAC0        0x0000
#define MAC1        0x0001
#define MAC2        0x0002
#define MAC3        0x0003
#define MAC4        0x0004
#define MAC5        0x0005
#define MAR0        0x0008
#define MAR1        0x0009
#define MAR2        0x000A
#define MAR3        0x000B
#define MAR4        0x000C
#define MAR5        0x000D
#define MAR6        0x000E
#define MAR7        0x000F
#define TSD0        0x0010
#define TSD1        0x0014
#define TSD2        0x0018
#define TSD3        0x001C
#define TSAD0       0x0020
#define TSAD1       0x0024
#define TSAD2       0x0028
#define TSAD3       0x002C
#define RBSTART     0x0030
#define CMD         0x0037
#define IMR         0x003C
#define ISR         0x003E
#define REG_CONFIG_1 0x52

#define ROK 0x01
#define TOK 0x04

static uint16_t io_base;
static void* read_buf_base;

void write_reg(uint16_t reg, uint32_t val) {
    outl(io_base + reg, val);
}

uint32_t read_reg(uint16_t reg) {
    return inl(io_base + reg);
}

static bool busy_wr = false;
static bool busy_rd = false;
static bool send_working = false;
static bool recv_working = false;

__attribute__((interrupt))
void rtl8139_handler(void*) {
    uint32_t status = read_reg(ISR);
    write_reg(ISR, status);
    
    if (status & TOK) busy_wr = false;
    if (status & ROK) busy_rd = false;
}

bool rtl8139_send(const uint8_t* data, size_t length) {
    while (send_working);
    send_working = true;
    
    busy_wr = true;
    
    write_reg(TSAD0, (uint32_t)(uint64_t)data);
    write_reg(TSD0, length & 0x1FFF);
    
    while (busy_wr);
    
    send_working = false;
    
    return true;
}

size_t rtl8139_recv(uint8_t* buffer, size_t len) {
    while (recv_working);
    recv_working = true;
    
    busy_rd = true;
    
    uint8_t* rx_ptr = (uint8_t*)read_buf_base;
    
    size_t copy_len = len < 2048 ? len : 2048;
    for (size_t i = 0; i < copy_len; i++)
    	buffer[i] = rx_ptr[i];
    
    busy_rd = false;
    recv_working = false;
    
    return copy_len;
}

size_t rtl8139_lstn(uint8_t* buffer, size_t len) {
    return rtl8139_recv(buffer, len);
}

bool rtl8139_get_mac(uint8_t _mac[6]) {
    _mac[0] = read_reg(MAC0) & 0xFF;
    _mac[1] = read_reg(MAC1) & 0xFF;
    _mac[2] = read_reg(MAC2) & 0xFF;
    _mac[3] = read_reg(MAC3) & 0xFF;
    _mac[4] = read_reg(MAC4) & 0xFF;
    _mac[5] = read_reg(MAC5) & 0xFF;
    
    return true;
}

void rtl8139_init(pcie_device* dev, net_card_driver* driver) {
    driver->name = "RTL8139";
    driver->send = rtl8139_send;
    driver->receive = rtl8139_recv;
    driver->listen = rtl8139_lstn;
    driver->get_mac = rtl8139_get_mac;

    dev->command |= (1 << 2); // PCI master

    io_base = dev->bars[0] & 0x3;

    write_reg(REG_CONFIG_1, 0x00);
    write_reg(CMD, 0x10);
    while ((read_reg(CMD) & 0x10) != 0) asm("pause");

    read_buf_base = mem::pmm::palloc(16);
    if ((uint64_t)read_buf_base > 0xFFFFFFFF - (0x1000 * 16))
        panic("read buffer is higher than 32-bits");

    write_reg(RBSTART, (uint32_t)(uint64_t)read_buf_base);

    write_reg(IMR, ROK | TOK);

    write_reg(0x44, 0x8F);
    write_reg(CMD, 0x0C);

    arch::x86_64::cpu::idt::set_descriptor(0x20 + dev->interrupt_line, (uint64_t)rtl8139_handler, 0x8E);
    printf("interrupt_line = %X\n\r", dev->interrupt_line);
}

