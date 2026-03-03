#include "e1000.hpp"
#include <cstdio>
#include <mmioutils.hpp>
#include <arch/arch.hpp>
#include <mem/mem.hpp>
#include <cstring>
#include <drivers/timers/apic/apic.hpp>

#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EEPROM      0x0014
#define REG_CTRL_EXT    0x0018
#define REG_IMASK       0x00D0
#define REG_RCTRL       0x0100
#define REG_RXDESCLO    0x2800
#define REG_RXDESCHI    0x2804
#define REG_RXDESCLEN   0x2808
#define REG_RXDESCHEAD  0x2810
#define REG_RXDESCTAIL  0x2818

#define REG_TCTRL       0x0400
#define REG_TXDESCLO    0x3800
#define REG_TXDESCHI    0x3804
#define REG_TXDESCLEN   0x3808
#define REG_TXDESCHEAD  0x3810
#define REG_TXDESCTAIL  0x3818


#define REG_RDTR         0x2820 // RX Delay Timer Register
#define REG_RXDCTL       0x2828 // RX Descriptor Control
#define REG_RADV         0x282C // RX Int. Absolute Delay Timer
#define REG_RSRPD        0x2C00 // RX Small Packet Detect Interrupt



#define REG_TIPG         0x0410      // Transmit Inter Packet Gap
#define ECTRL_SLU        0x40        //set link up


#define RCTL_EN                         (1 << 1)    // Receiver Enable
#define RCTL_SBP                        (1 << 2)    // Store Bad Packets
#define RCTL_UPE                        (1 << 3)    // Unicast Promiscuous Enabled
#define RCTL_MPE                        (1 << 4)    // Multicast Promiscuous Enabled
#define RCTL_LPE                        (1 << 5)    // Long Packet Reception Enable
#define RCTL_LBM_NONE                   (0 << 6)    // No Loopback
#define RCTL_LBM_PHY                    (3 << 6)    // PHY or external SerDesc loopback
#define RTCL_RDMTS_HALF                 (0 << 8)    // Free Buffer Threshold is 1/2 of RDLEN
#define RTCL_RDMTS_QUARTER              (1 << 8)    // Free Buffer Threshold is 1/4 of RDLEN
#define RTCL_RDMTS_EIGHTH               (2 << 8)    // Free Buffer Threshold is 1/8 of RDLEN
#define RCTL_MO_36                      (0 << 12)   // Multicast Offset - bits 47:36
#define RCTL_MO_35                      (1 << 12)   // Multicast Offset - bits 46:35
#define RCTL_MO_34                      (2 << 12)   // Multicast Offset - bits 45:34
#define RCTL_MO_32                      (3 << 12)   // Multicast Offset - bits 43:32
#define RCTL_BAM                        (1 << 15)   // Broadcast Accept Mode
#define RCTL_VFE                        (1 << 18)   // VLAN Filter Enable
#define RCTL_CFIEN                      (1 << 19)   // Canonical Form Indicator Enable
#define RCTL_CFI                        (1 << 20)   // Canonical Form Indicator Bit Value
#define RCTL_DPF                        (1 << 22)   // Discard Pause Frames
#define RCTL_PMCF                       (1 << 23)   // Pass MAC Control Frames
#define RCTL_SECRC                      (1 << 26)   // Strip Ethernet CRC

// Buffer Sizes
#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))


// Transmit Command

#define CMD_EOP                         (1 << 0)    // End of Packet
#define CMD_IFCS                        (1 << 1)    // Insert FCS
#define CMD_IC                          (1 << 2)    // Insert Checksum
#define CMD_RS                          (1 << 3)    // Report Status
#define CMD_RPS                         (1 << 4)    // Report Packet Sent
#define CMD_VLE                         (1 << 6)    // VLAN Packet Enable
#define CMD_IDE                         (1 << 7)    // Interrupt Delay Enable


// TCTL Register

#define TCTL_EN                         (1 << 1)    // Transmit Enable
#define TCTL_PSP                        (1 << 3)    // Pad Short Packets
#define TCTL_CT_SHIFT                   4           // Collision Threshold
#define TCTL_COLD_SHIFT                 12          // Collision Distance
#define TCTL_SWXOFF                     (1 << 22)   // Software XOFF Transmission
#define TCTL_RTLC                       (1 << 24)   // Re-transmit on Late Collision

#define TSTA_DD                         (1 << 0)    // Descriptor Done
#define TSTA_EC                         (1 << 1)    // Excess Collisions
#define TSTA_LC                         (1 << 2)    // Late Collision
#define LSTA_TU                         (1 << 3)    // Transmit Underrun

#define E1000_NUM_RX_DESC 128
#define E1000_NUM_TX_DESC 8

struct e1000_rx_desc {
        volatile uint64_t addr;
        volatile uint16_t length;
        volatile uint16_t checksum;
        volatile uint8_t status;
        volatile uint8_t errors;
        volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
        volatile uint64_t addr;
        volatile uint16_t length;
        volatile uint8_t cso;
        volatile uint8_t cmd;
        volatile uint8_t status;
        volatile uint8_t css;
        volatile uint16_t special;
} __attribute__((packed));

/* driver code */

uint8_t bar_type;
uint16_t io_base;
uint64_t mem_base;
bool eeprom_exists;
uint8_t mac[6];

e1000_rx_desc* rx_descs[E1000_NUM_RX_DESC];
e1000_tx_desc* tx_descs[E1000_NUM_TX_DESC];

uint8_t* rx_virt_addrs[E1000_NUM_RX_DESC];

volatile uint16_t rx_cur;
volatile uint16_t tx_cur;
volatile uint8_t interrupt_line;

void write_command(uint16_t p_address, uint32_t p_value) {
	if (bar_type == 0) {
		mmioutils::write32(mem_base + p_address, p_value);
	} else {
		arch::x86_64::io::outl(io_base, p_address);
		arch::x86_64::io::outl(io_base + 4, p_value);
	}
}

uint32_t read_command(uint16_t p_address) {
	if (bar_type == 0) {
		return mmioutils::read32(mem_base + p_address);
	} else {
		arch::x86_64::io::outl(io_base, p_address);
		return arch::x86_64::io::inl(io_base + 4);
	}
}

bool detect_eeprom() {
	uint32_t val = 0;
	write_command(REG_EEPROM, 0x1);

	for (int i = 0; i < 1000 && !eeprom_exists; i++) {
		val = read_command(REG_EEPROM);
		if (val & 0x10) {
			eeprom_exists = true;
		} else {
			eeprom_exists = false;
		}
	}
	return eeprom_exists;
}

uint32_t eeprom_read(uint8_t addr) {
	uint16_t data = 0;
	uint32_t tmp = 0;

	if (eeprom_exists) {
		write_command(REG_EEPROM, (1) | ((uint32_t)(addr << 8)));

		while (!((tmp = read_command(REG_EEPROM)) & (1 << 4)));
	} else {
		write_command(REG_EEPROM, (1) | ((uint32_t)(addr) << 2));

		while (!((tmp = read_command(REG_EEPROM)) & (1 << 1)));
	}

	data = (uint16_t)((tmp >> 16) & 0xFFFF);
	return data;
}

bool read_mac_address() {
	if (eeprom_exists) {
		uint32_t temp;
		temp = eeprom_read(0);
		mac[0] = temp & 0xFF;
		mac[1] = temp >> 8;
		temp = eeprom_read(1);
		mac[2] = temp & 0xFF;
		mac[3] = temp >> 8;
		temp = eeprom_read(2);
		mac[4] = temp & 0xFF;
		mac[5] = temp >> 8;
	} else {
		uint8_t* mem_base_mac_8 = (uint8_t*)(mem_base + 0x5400);
		uint32_t* mem_base_mac_32 = (uint32_t*)(mem_base + 0x5400);

		if (mem_base_mac_32[0] != 0) {
			for (int i = 0; i < 6; i++) {
				mac[i] = mem_base_mac_8[i];
			}
		} else {
			return false;
		}
	}
	return true;
}

void start_link() {
	uint32_t val = read_command(REG_CTRL);
	write_command(REG_CTRL, val | ECTRL_SLU);
}

void rx_init() {
    uint8_t* ptr;
    struct e1000_rx_desc* descs;

    ptr = (uint8_t*)mem::heap::malloc(sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC + 16);

    descs = (struct e1000_rx_desc*)ptr;
    for(int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i] = (struct e1000_rx_desc*)((uint8_t*)descs + i * 16);
        uint8_t* buf = (uint8_t*)mem::heap::malloc(8192 + 16);
        rx_virt_addrs[i] = buf;
        rx_descs[i]->addr = mem::vmm::va_to_pa((uint64_t)buf);
        rx_descs[i]->status = 0;
    }

    uint64_t phys_ptr = mem::vmm::va_to_pa((uint64_t)ptr);
	write_command(REG_RXDESCLO, (uint32_t)(phys_ptr & 0xFFFFFFFF));
	write_command(REG_RXDESCHI, (uint32_t)(phys_ptr >> 32));

    write_command(REG_RXDESCLEN, E1000_NUM_RX_DESC * 16);

    write_command(REG_RXDESCHEAD, 0);
    write_command(REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);
    rx_cur = 0;
    write_command(REG_RCTRL, RCTL_EN | RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BAM | RCTL_SECRC | RCTL_BSIZE_8192);
}

void tx_init() {
    uint8_t *ptr;
    struct e1000_tx_desc *descs;

    ptr = (uint8_t *)mem::heap::malloc(sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC + 16);

    descs = (struct e1000_tx_desc*)ptr;
    for(int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i] = (struct e1000_tx_desc*)((uint8_t*)descs + i*16);
        tx_descs[i]->addr = 0;
        tx_descs[i]->cmd = 0;
        tx_descs[i]->status = TSTA_DD;
    }

    uint64_t phys_ptr = mem::vmm::va_to_pa((uint64_t)ptr);
    write_command(REG_TXDESCHI, (uint32_t)(phys_ptr >> 32) );
    write_command(REG_TXDESCLO, (uint32_t)(phys_ptr & 0xFFFFFFFF));

    write_command(REG_TXDESCLEN, E1000_NUM_TX_DESC * 16);

    write_command( REG_TXDESCHEAD, 0);
    write_command( REG_TXDESCTAIL, 0);
    tx_cur = 0;
    write_command(REG_TCTRL,  TCTL_EN
        | TCTL_PSP
        | (15 << TCTL_CT_SHIFT)
        | (64 << TCTL_COLD_SHIFT)
        | TCTL_RTLC);

    write_command(REG_TIPG, 0x0060200A);
}

void enable_interrupt() {
	write_command(REG_IMASK, 0x1F6DC);
	write_command(REG_IMASK, 0xFF & ~4);
	read_command(0xC0);
}

volatile bool receive_pending = false;

void handle_receive() {
    receive_pending = true;
}

volatile bool transmit_done = true;

__attribute__((interrupt))
void fire(void* frame) {
	write_command(REG_IMASK, 0x1);

	uint32_t status = read_command(0xC0);

	printf("interrupt! %08X\n\r", status);

	if (status & 0x04) {
		start_link();
	} else if (status & 0x03) {
		transmit_done = true;
	} else if (status & 0x90) {
		handle_receive();
	}

	arch::x86_64::cpu::idt::send_eoi(interrupt_line);
}

int send_packet(const void* p_data, uint16_t p_len) {
	while (transmit_done == false);
	transmit_done = false;

    uint64_t phys_addr = mem::vmm::va_to_pa((uint64_t)p_data);
    tx_descs[tx_cur]->addr = phys_addr;
    tx_descs[tx_cur]->length = p_len;
    tx_descs[tx_cur]->cmd = CMD_EOP | CMD_IFCS | CMD_RS;
    tx_descs[tx_cur]->status = 0;
    uint8_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    write_command(REG_TXDESCTAIL, tx_cur);
    
    while(!(tx_descs[old_cur]->status & TSTA_DD)) {
    	if (transmit_done) break;
        __asm__ volatile("pause");
    }
    
    return (int)(tx_descs[old_cur]->status & (TSTA_EC | TSTA_LC));
}

bool e1000_send(const uint8_t* data, size_t length);
size_t e1000_recv(uint8_t* buffer, size_t len);
size_t e1000_lstn(uint8_t* buffer, size_t len);
bool e1000_get_mac(uint8_t _mac[6]);

#define E1000_CTRL 0x0000
#define E1000_CTRL_RST (1 << 26)

void e1000_reset() {
    write_command(REG_CTRL, read_command(REG_CTRL) | (1 << 26));
    while (read_command(REG_CTRL) & (1 << 26)) asm("pause");
}

void e1000_init(pcie_device* dev, net_card_driver* driver) {
	printf("Initialising E1000 network card\n\r");

	driver->name = "E1000";
	driver->send = e1000_send;
	driver->receive = e1000_recv;
	driver->listen = e1000_lstn;
	driver->get_mac = e1000_get_mac;

	pcie::enable_bus_mastering(dev);

	bar_type = dev->bars[0] & 0x1;
	io_base = dev->bars[0] & ~0x3;
	if (((dev->bars[0] >> 1) & 0x3) == 0x2) {
		mem_base = ((uint64_t)dev->bars[1] << 32) | (dev->bars[0] & ~0xFULL);
	} else {
		mem_base = dev->bars[0] & ~0xFULL;
	}

	eeprom_exists = false;

	detect_eeprom();

	if (!read_mac_address()) panic("MAC not present");

	printf("MAC Address:\n");
	for (int i = 0; i < 6; i++) {
	    printf("%02X", mac[i]);
	    if (i < 5) {
	        printf(":");
	    }
	}
	printf("\n\r");

	e1000_reset();

	start_link();

	for (int i = 0; i < 0x80; i++)
		write_command(0x5200 + i * 4, 0);

	interrupt_line = dev->interrupt_line;
	arch::x86_64::cpu::idt::set_descriptor(0x20 + dev->interrupt_line, (uint64_t)fire, 0x8E);
	arch::x86_64::cpu::idt::send_eoi(dev->interrupt_line);
	arch::x86_64::cpu::idt::irq_clear_mask(dev->interrupt_line);
	printf("vector = %02X\n\r", 0x20 + dev->interrupt_line);

	enable_interrupt();
	rx_init();
	tx_init();
}

bool e1000_send(const uint8_t* data, size_t length) {
	int ret = send_packet(data, length);
	if (ret == 0) return true;
	else return false;
}

size_t e1000_recv(uint8_t* buffer, size_t len) {
    e1000_rx_desc* desc = rx_descs[rx_cur];

    if (!(desc->status & 0x1)) {
        return 0;
    }

    size_t pkt_len = desc->length;
    if (pkt_len > len) pkt_len = len;

    mem::memcpy(buffer, rx_virt_addrs[rx_cur], pkt_len);

    desc->status = 0;

    uint16_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    write_command(REG_RXDESCTAIL, old_cur);

    return pkt_len;
}

size_t e1000_lstn(uint8_t* buffer, size_t len) {
    size_t received = 0;
    while ((received = e1000_recv(buffer, len)) == 0) {
        drivers::timers::apic::sleep_ms(1);
    }
    return received;
}

bool e1000_get_mac(uint8_t _mac[6]) {
	if (!_mac) return false;
	for (int i = 0; i < 6; i++) {
		_mac[i] = mac[i];
	}
	return true;
}
