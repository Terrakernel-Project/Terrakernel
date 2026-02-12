#include "netgeneric.hpp"
#include <panic.hpp>
#include <mem/mem.hpp>
#include "dhcp.hpp"
#include "e1000/e1000.hpp"
#include "rtl8139/rtl8139.hpp"

net_card_driver* driver;
pcie_device* device;

struct net_card {
	uint16_t vendor;
	uint16_t device;
};

net_stats stats;

#define MAX_CARDS 75

net_card net_cards[MAX_CARDS] = {
    {0x8086, 0x1000}, // Intel 82542 Gigabit Ethernet
    {0x8086, 0x1001}, // Intel 82543GC
    {0x8086, 0x1004}, // Intel 82544GC
    {0x8086, 0x1008}, // Intel 82544EI (Copper)
    {0x8086, 0x1009}, // Intel 82544EI (Fiber)
    {0x8086, 0x100C}, // Intel 82544GC (Copper)
    {0x8086, 0x100D}, // Intel 82544GC (LOM)
    {0x8086, 0x100E}, // Intel 82540EM
    {0x8086, 0x100F}, // Intel 82545EM
    {0x8086, 0x1010}, // Intel 82546EB
    {0x8086, 0x1011}, // Intel 82545EM (Fiber)
    {0x8086, 0x1012}, // Intel 82546EB (Fiber)
    {0x8086, 0x1013}, // Intel 82541EI
    {0x8086, 0x1014}, // Intel 82541ER
    {0x8086, 0x1015}, // Intel 82540EM (LOM)
    {0x8086, 0x1016}, // Intel 82540EP
    {0x8086, 0x1017}, // Intel 82540EP Variant
    {0x8086, 0x1018}, // Intel 82541EI Mobile
    {0x8086, 0x1019}, // Intel 82547EI
    {0x8086, 0x101A}, // Intel 82544GC Variant
    {0x8086, 0x101D}, // Intel 82547EI Variant
    {0x8086, 0x101E}, // Intel 82540EP LOM
    {0x8086, 0x1026}, // Intel 82545GM
    {0x8086, 0x1027}, // Intel 82547EI (Alt)
    {0x8086, 0x1028}, // Intel 82547EI (Alt)
    {0x8086, 0x1075}, // Intel PRO/1000 GT
    {0x8086, 0x1076}, // Intel PRO/1000 Variant
    {0x8086, 0x1077}, // Intel PRO/1000 Variant
    {0x8086, 0x1078}, // Intel PRO/1000 Variant
    {0x8086, 0x1079}, // Intel PRO/1000 Variant
    {0x8086, 0x107A}, // Intel PRO/1000 Variant
    {0x8086, 0x107B}, // Intel PRO/1000 Variant
    {0x8086, 0x107C}, // Intel PRO/1000 GT Desktop Adapter (82541PI)
    {0x8086, 0x108A}, // Intel 82567V Gigabit Network Connection
    {0x8086, 0x1099}, // Intel 82546GB Gigabit Ethernet
    {0x8086, 0x10B5}, // Intel 82545GM
    {0x8086, 0x1049}, // Intel 82566MM Gigabit
    {0x8086, 0x1501}, // Intel 82567V-3 Gigabit
    {0x8086, 0x1502}, // Intel 82579LM Gigabit
    {0x8086, 0x1503}, // Intel 82579V Gigabit
    {0x8086, 0x150C}, // Intel 82583V Gigabit
    {0x8086, 0x1525}, // Intel 82567V-4 Gigabit
    {0x8086, 0x153A}, // Intel Ethernet Connection I217-LM
    {0x8086, 0x153B}, // Intel Ethernet Connection I217-V
    {0x8086, 0x1559}, // Intel Ethernet Connection I218-V
    {0x8086, 0x155A}, // Intel Ethernet Connection I218-LM
    {0x8086, 0x156F}, // Intel Ethernet Connection I219-LM
    {0x8086, 0x1570}, // Intel Ethernet Connection I219-V
    {0x8086, 0x15A0}, // Intel Ethernet Connection (2) I218-LM
    {0x8086, 0x15A1}, // Intel Ethernet Connection (2) I218-V
    {0x8086, 0x15A2}, // Intel Ethernet Connection (3) I218-LM
    {0x8086, 0x15A3}, // Intel Ethernet Connection (3) I218-V
    {0x8086, 0x15B7}, // Intel Ethernet Connection (2) I219-LM
    {0x8086, 0x15B8}, // Intel Ethernet Connection (2) I219-V
    {0x8086, 0x15BB}, // Intel Ethernet Connection (7) I219-LM
    {0x8086, 0x15BC}, // Intel Ethernet Connection (7) I219-V
    {0x8086, 0x15BD}, // Intel Ethernet Connection (6) I219-LM
    {0x8086, 0x15BE}, // Intel Ethernet Connection (6) I219-V
    {0x8086, 0x15DF}, // Intel Ethernet Connection (8) I219-LM
    {0x8086, 0x15E0}, // Intel Ethernet Connection (8) I219-V
    {0x8086, 0x15E1}, // Intel Ethernet Connection (9) I219-LM
    {0x8086, 0x15E2}, // Intel Ethernet Connection (9) I219-V
    {0x8086, 0x15F4}, // Intel Ethernet Connection (15) I219-LM
    {0x8086, 0x15F5}, // Intel Ethernet Connection (15) I219-V
    {0x8086, 0x15F9}, // Intel Ethernet Connection (14) I219-LM
    {0x8086, 0x15FA}, // Intel Ethernet Connection (14) I219-V
    {0x10EC, 0x8139}, // RTL-8139 / RTL8139C / RTL8139C+ / RTL8139D (Fast Ethernet)
    {0x10EC, 0x8138}, // RTL8139 (Cardbus / variant)
    {0x10EC, 0x8136}, // RTL8101E family fast Ethernet
    {0x10EC, 0x8129}, // RTL-8129
    {0x10EC, 0x8137}, // RTL8104E fast Ethernet controller
    {0xFFFF, 0xFFFF} // INVALID
};

void (*init_card_funcs[MAX_CARDS])(pcie_device*,net_card_driver*) = {
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
	rtl8139_init,
	rtl8139_init,
	rtl8139_init,
	rtl8139_init,
	rtl8139_init,
	nullptr
};

namespace drivers::net::netgeneric {

static ip_u ip_address;

void initialise() {
	int idx;
	for (int i = 0; i < MAX_CARDS; i++) {
		idx = i;
		device = pcie::get_device_vendor_id(net_cards[i].vendor, net_cards[i].device);
		if (device) break;
	}

	if (!device) {
		panic("no net device present or no supported net device");
	}

	driver = (net_card_driver*)mem::heap::malloc(sizeof(net_card_driver));

	init_card_funcs[idx](device, driver);

	uint32_t transaction_id = 0x12345678;

	mac_u mac;
	get_mac(mac.mac_p);

	ip_address = dhcp_request(transaction_id, "Terrakernel", mac);
}

bool send(const uint8_t* data, size_t length) {
	stats.packets_sent++;
	if (driver->send(data, length)) {
		return true;
	} else {
		stats.errors++;
		return false;
	}
}

size_t recv(uint8_t* buffer, size_t buffer_len) {
	stats.packets_received++;
	if (driver->receive(buffer, buffer_len)) {
		return true;
	} else {
		stats.errors++;
		return false;
	}
}

size_t listen(uint8_t* buffer, size_t buffer_len) {
	stats.packets_received++;
	return driver->listen(buffer, buffer_len);
}

bool get_mac(uint8_t mac[6]) {
	if (driver->get_mac(mac)) {
		return true;
	} else {
		stats.errors++;
		return false;
	}
}

net_stats net_card_get_stats() {
	return stats;
}

}
