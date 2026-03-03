#include "arp.hpp"
#include "../netgeneric.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <cstdio>

struct ethernet_frame {
	uint8_t dst_mac[6];
	uint8_t src_mac[6];
	uint16_t ethertype;
} __attribute__((packed));

struct arp_packet {
	uint16_t htype;
	uint16_t ptype;
	uint8_t  hlen;
	uint8_t  plen;
	uint16_t oper;
	uint8_t  sha[6];
	uint32_t spa;
	uint8_t  tha[6];
	uint32_t tpa;
} __attribute__((packed));

#define ARP_HTYPE_ETHERNET  0x0001
#define ARP_PTYPE_IPV4      0x0800
#define ARP_OPER_REQUEST    0x0001
#define ARP_OPER_REPLY      0x0002
#define ETHERTYPE_ARP       0x0806

#define ARP_CACHE_SIZE 16

struct arp_cache_entry {
	uint32_t ip;
	uint8_t  mac[6];
	bool     valid;
};

static arp_cache_entry arp_cache[ARP_CACHE_SIZE];
static uint32_t arp_src_ip = 0;

void arp_set_src_ip(ip_u src_ip) {
	arp_src_ip = src_ip.ip;
}

static void arp_cache_store(uint32_t ip, uint8_t mac[6]) {
	for (int i = 0; i < ARP_CACHE_SIZE; i++) {
		if (!arp_cache[i].valid || arp_cache[i].ip == ip) {
			arp_cache[i].ip = ip;
			mem::memcpy(arp_cache[i].mac, mac, 6);
			arp_cache[i].valid = true;
			return;
		}
	}
	arp_cache[0].ip = ip;
	mem::memcpy(arp_cache[0].mac, mac, 6);
	arp_cache[0].valid = true;
}

static bool arp_cache_lookup(uint32_t ip, uint8_t mac_out[6]) {
	for (int i = 0; i < ARP_CACHE_SIZE; i++) {
		if (arp_cache[i].valid && arp_cache[i].ip == ip) {
			mem::memcpy(mac_out, arp_cache[i].mac, 6);
			return true;
		}
	}
	return false;
}

namespace drivers::net::arp {

void arp_handle_packet(uint8_t* frame, size_t length) {
	if (length < sizeof(ethernet_frame) + sizeof(arp_packet)) return;

	arp_packet* arp = (arp_packet*)(frame + sizeof(ethernet_frame));

	if (__builtin_bswap16(arp->htype) != ARP_HTYPE_ETHERNET) return;
	if (__builtin_bswap16(arp->ptype) != ARP_PTYPE_IPV4) return;
	if (__builtin_bswap16(arp->oper)  != ARP_OPER_REPLY) return;

	arp_cache_store(arp->spa, arp->sha);
}

bool arp_lookup(ip_u target_ip, uint8_t mac_out[6]) {
	if (arp_cache_lookup(target_ip.ip, mac_out)) return true;

	uint8_t src_mac[6];
	drivers::net::netgeneric::get_mac(src_mac);

	size_t total = sizeof(ethernet_frame) + sizeof(arp_packet);
	uint8_t frame[total];
	mem::memset(frame, 0, total);

	ethernet_frame* eth = (ethernet_frame*)frame;
	mem::memset(eth->dst_mac, 0xFF, 6);
	mem::memcpy(eth->src_mac, src_mac, 6);
	eth->ethertype = __builtin_bswap16(ETHERTYPE_ARP);

	arp_packet* arp = (arp_packet*)(frame + sizeof(ethernet_frame));
	arp->htype = __builtin_bswap16(ARP_HTYPE_ETHERNET);
	arp->ptype = __builtin_bswap16(ARP_PTYPE_IPV4);
	arp->hlen  = 6;
	arp->plen  = 4;
	arp->oper  = __builtin_bswap16(ARP_OPER_REQUEST);
	mem::memcpy(arp->sha, src_mac, 6);
	arp->spa = arp_src_ip;
	mem::memset(arp->tha, 0x00, 6);
	arp->tpa = target_ip.ip;

	drivers::net::netgeneric::send(frame, total);

	uint8_t buffer[2048];
	for (int attempts = 0; attempts < 1000; attempts++) {
		size_t received = drivers::net::netgeneric::listen(buffer, sizeof(buffer));
		if (received < sizeof(ethernet_frame) + sizeof(arp_packet)) continue;

		ethernet_frame* reth = (ethernet_frame*)buffer;
		if (__builtin_bswap16(reth->ethertype) != ETHERTYPE_ARP) continue;

		arp_packet* rarp = (arp_packet*)(buffer + sizeof(ethernet_frame));
		if (__builtin_bswap16(rarp->oper) != ARP_OPER_REPLY) continue;
		if (rarp->spa != target_ip.ip) continue;

		mem::memcpy(mac_out, rarp->sha, 6);
		arp_cache_store(target_ip.ip, rarp->sha);

		printf("arp: %d.%d.%d.%d is at %02X:%02X:%02X:%02X:%02X:%02X\n\r",
			target_ip.ip_p[0], target_ip.ip_p[1],
			target_ip.ip_p[2], target_ip.ip_p[3],
			mac_out[0], mac_out[1], mac_out[2],
			mac_out[3], mac_out[4], mac_out[5]);

		return true;
	}

	printf("arp: no reply for %d.%d.%d.%d\n\r",
		target_ip.ip_p[0], target_ip.ip_p[1],
		target_ip.ip_p[2], target_ip.ip_p[3]);
	return false;
}

}
