#include "netgeneric.hpp"
#include <pcie/pcie.hpp>
#include <panic.hpp>
#include <mem/mem.hpp>
#include "e1000/e1000.hpp"
#include "dhcp.hpp"

net_card_driver* driver;
pcie_device* device;

struct net_card {
	uint16_t vendor;
	uint16_t device;
};

net_stats stats;

#define MAX_CARDS 5

net_card net_cards[MAX_CARDS] = {
	{0x8086, 0x10D3}, // Intel PRO/1000 MT Desktop Adapter
	{0x8086, 0x100E}, // Intel PRO/1000 MT Server Adapter
	{0x8086, 0x153A}, // E1000 I217
	{0x8086, 0x10EA}, // E1000 82577LM
	{0xFFFF, 0xFFFF}  // INVALID
};

void (*init_card_funcs[MAX_CARDS])(pcie_device*,net_card_driver*) = {
	e1000_init,
	e1000_init,
	e1000_init,
	e1000_init,
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
	return driver->send(data, length);
}

size_t recv(uint8_t* buffer, size_t buffer_len) {
	stats.packets_received++;
	return driver->receive(buffer, buffer_len);
}

size_t listen(uint8_t* buffer, size_t buffer_len) {
	stats.packets_received++;
	return driver->listen(buffer, buffer_len);
}

bool get_mac(uint8_t mac[6]) {
	return driver->get_mac(mac);
}

net_stats net_card_get_stats() {
	return stats;
}

}
