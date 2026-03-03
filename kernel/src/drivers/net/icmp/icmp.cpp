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

struct icmp_header {
	uint8_t  type;
	uint8_t  code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence;
} __attribute__((packed));

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

static ip_u icmp_src_ip = {.ip = 0};
static uint16_t icmp_identifier = 0x1337;

static ip_u icmp_gateway_ip = {.ip = 0};

void icmp_set_config(ip_u src_ip, ip_u gateway_ip) {
	icmp_src_ip    = src_ip;
	icmp_gateway_ip = gateway_ip;
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

namespace drivers::net::icmp {

bool icmp_ping(ip_u target_ip, uint16_t sequence, uint32_t* rtt_ms) {
	size_t payload_len = 32;
	size_t icmp_len    = sizeof(icmp_header) + payload_len;
	size_t total       = sizeof(ethernet_frame) + sizeof(ip_header) + icmp_len;

	uint8_t* frame = (uint8_t*)mem::heap::malloc(total);
	if (!frame) return false;
	mem::memset(frame, 0, total);

	uint8_t src_mac[6];
	drivers::net::netgeneric::get_mac(src_mac);

	ip_u arp_target;
	if ((target_ip.ip & 0x00FFFFFF) == (icmp_src_ip.ip & 0x00FFFFFF)) {
		arp_target = target_ip;
	} else {
		arp_target = icmp_gateway_ip;
	}

	uint8_t dst_mac[6];
	if (!drivers::net::arp::arp_lookup(arp_target, dst_mac)) return false;

	ethernet_frame* eth = (ethernet_frame*)frame;
	mem::memcpy(eth->dst_mac, dst_mac, 6);
	mem::memcpy(eth->src_mac, src_mac, 6);
	eth->ethertype = __builtin_bswap16(0x0800);

	ip_header* ip = (ip_header*)(frame + sizeof(ethernet_frame));
	size_t ip_size = sizeof(ip_header) + icmp_len;
	ip->version_ihl    = 0x45;
	ip->total_length   = __builtin_bswap16(ip_size);
	ip->identification = __builtin_bswap16(0x1337);
	ip->ttl            = 64;
	ip->protocol       = 1;
	ip->src_ip         = icmp_src_ip.ip;
	ip->dst_ip         = target_ip.ip;
	ip->checksum       = ip_checksum(ip, sizeof(ip_header));

	icmp_header* icmp = (icmp_header*)((uint8_t*)ip + sizeof(ip_header));
	icmp->type       = ICMP_ECHO_REQUEST;
	icmp->code       = 0;
	icmp->identifier = __builtin_bswap16(icmp_identifier);
	icmp->sequence   = __builtin_bswap16(sequence);
	icmp->checksum   = 0;

	uint8_t* payload = (uint8_t*)icmp + sizeof(icmp_header);
	for (size_t i = 0; i < payload_len; i++) payload[i] = (uint8_t)i;

	icmp->checksum = ip_checksum(icmp, icmp_len);

	bool ok = drivers::net::netgeneric::send(frame, total);
	mem::heap::free(frame);
	if (!ok) return false;

	uint8_t buffer[2048];
	while (true) {
		size_t received = drivers::net::netgeneric::listen(buffer, sizeof(buffer));
		if (received < sizeof(ethernet_frame) + sizeof(ip_header) + sizeof(icmp_header)) continue;

		ethernet_frame* reth = (ethernet_frame*)buffer;
		if (__builtin_bswap16(reth->ethertype) != 0x0800) continue;

		ip_header* rip = (ip_header*)(buffer + sizeof(ethernet_frame));
		if ((rip->version_ihl & 0xF0) != 0x40) continue;
		if (rip->protocol != 1) continue;
		if (rip->src_ip != target_ip.ip) continue;

		size_t ip_hdr_len = (rip->version_ihl & 0x0F) * 4;
		icmp_header* ricmp = (icmp_header*)((uint8_t*)rip + ip_hdr_len);
		if (ricmp->type != ICMP_ECHO_REPLY) continue;
		if (ricmp->code != 0) continue;
		if (__builtin_bswap16(ricmp->identifier) != icmp_identifier) continue;
		if (__builtin_bswap16(ricmp->sequence) != sequence) continue;

		return true;
	}
}

void icmp_ping_print(ip_u target_ip, int count) {
	printf("PING %d.%d.%d.%d\n\r",
		target_ip.ip_p[0], target_ip.ip_p[1],
		target_ip.ip_p[2], target_ip.ip_p[3]);

	int success = 0;
	for (int i = 0; i < count; i++) {
		bool ok = icmp_ping(target_ip, (uint16_t)i, nullptr);
		if (ok) {
			success++;
			printf("Reply from %d.%d.%d.%d: seq=%d\n\r",
				target_ip.ip_p[0], target_ip.ip_p[1],
				target_ip.ip_p[2], target_ip.ip_p[3], i);
		} else {
			printf("Request timeout for seq=%d\n\r", i);
		}
	}

	printf("%d packets transmitted, %d received, %d%% packet loss\n\r",
		count, success, ((count - success) * 100) / count);
}

}
