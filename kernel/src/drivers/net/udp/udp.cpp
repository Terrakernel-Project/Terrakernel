#include "../dhcp.hpp"
#include "../netgeneric.hpp"
#include "../arp/arp.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <cstdio>

struct ethernet_frame {
	uint8_t dst_mac[6];
	uint8_t src_mac[6];
	uint16_t ethertype;
} __attribute__((packed));

struct ip_header {
	uint8_t version_ihl;
	uint8_t dscp_ecn;
	uint16_t total_length;
	uint16_t identification;
	uint16_t flags_fragment;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	uint32_t src_ip;
	uint32_t dst_ip;
} __attribute__((packed));

struct udp_header {
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t length;
	uint16_t checksum;
} __attribute__((packed));

static ip_u udp_src_ip    = {.ip = 0};
static ip_u udp_gateway_ip = {.ip = 0};
static uint16_t udp_src_port = 0;
static uint16_t udp_dst_port = 0;

void udp_set_config(ip_u src_ip, uint16_t src_port, uint16_t dst_port) {
	udp_src_ip   = src_ip;
	udp_src_port = src_port;
	udp_dst_port = dst_port;
}

void udp_set_gateway(ip_u gateway_ip) {
	udp_gateway_ip = gateway_ip;
}

static uint16_t ip_checksum(void* data, size_t len) {
	uint16_t* ptr = (uint16_t*)data;
	uint32_t sum = 0;
	while (len > 1) {
		sum += *ptr++;
		len -= 2;
	}
	if (len) sum += *(uint8_t*)ptr;
	while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
	return ~sum;
}

namespace drivers::net::udp {

bool udp_send_packet(const uint8_t* data, size_t length, ip_u target_ip) {
	size_t total = sizeof(ethernet_frame) + sizeof(ip_header) + sizeof(udp_header) + length;
	uint8_t* frame = (uint8_t*)mem::heap::malloc(total);
	if (!frame) return false;
	mem::memset(frame, 0, total);

	uint8_t src_mac[6];
	drivers::net::netgeneric::get_mac(src_mac);

	ip_u arp_target;
	if ((target_ip.ip & 0x00FFFFFF) == (udp_src_ip.ip & 0x00FFFFFF)) {
		arp_target = target_ip;
	} else {
		arp_target = udp_gateway_ip;
	}

	uint8_t dst_mac[6];
	if (!drivers::net::arp::arp_lookup(arp_target, dst_mac)) {
		mem::heap::free(frame);
		return false;
	}

	ethernet_frame* eth = (ethernet_frame*)frame;
	mem::memcpy(eth->dst_mac, dst_mac, 6);
	mem::memcpy(eth->src_mac, src_mac, 6);
	eth->ethertype = __builtin_bswap16(0x0800);

	ip_header* ip = (ip_header*)(frame + sizeof(ethernet_frame));
	size_t ip_size = sizeof(ip_header) + sizeof(udp_header) + length;
	ip->version_ihl    = 0x45;
	ip->dscp_ecn       = 0;
	ip->total_length   = __builtin_bswap16(ip_size);
	ip->identification = __builtin_bswap16(0x1337);
	ip->flags_fragment = 0;
	ip->ttl            = 64;
	ip->protocol       = 17;
	ip->src_ip         = udp_src_ip.ip;
	ip->dst_ip         = target_ip.ip;
	ip->checksum       = 0;
	ip->checksum       = ip_checksum(ip, sizeof(ip_header));

	udp_header* udp = (udp_header*)((uint8_t*)ip + sizeof(ip_header));
	size_t udp_size = sizeof(udp_header) + length;
	udp->src_port = __builtin_bswap16(udp_src_port);
	udp->dst_port = __builtin_bswap16(udp_dst_port);
	udp->length   = __builtin_bswap16(udp_size);
	udp->checksum = 0;

	mem::memcpy((uint8_t*)udp + sizeof(udp_header), data, length);

	bool ok = drivers::net::netgeneric::send(frame, total);
	mem::heap::free(frame);
	return ok;
}

bool udp_recv_packet(uint8_t* buffer, size_t length, ip_u target_ip) {
	if (length < sizeof(ethernet_frame) + sizeof(ip_header) + sizeof(udp_header)) return false;

	ethernet_frame* eth = (ethernet_frame*)buffer;
	if (__builtin_bswap16(eth->ethertype) != 0x0800) return false;

	ip_header* ip = (ip_header*)(buffer + sizeof(ethernet_frame));
	if ((ip->version_ihl & 0xF0) != 0x40) return false;
	if (ip->protocol != 17) return false;
	if (target_ip.ip != 0 && ip->src_ip != target_ip.ip) return false;

	size_t ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
	udp_header* udp = (udp_header*)((uint8_t*)ip + ip_hdr_len);
	if (udp_dst_port != 0 && __builtin_bswap16(udp->dst_port) != udp_dst_port) return false;

	size_t payload_len = __builtin_bswap16(udp->length) - sizeof(udp_header);
	uint8_t* payload = (uint8_t*)udp + sizeof(udp_header);
	mem::memmove(buffer, payload, payload_len);

	return true;
}

bool udp_listen_packet(uint8_t* buffer, size_t length, ip_u target_ip) {
	while (true) {
		size_t received = drivers::net::netgeneric::listen(buffer, length);
		if (received == 0) continue;
		if (udp_recv_packet(buffer, received, target_ip)) return true;
	}
}

}
