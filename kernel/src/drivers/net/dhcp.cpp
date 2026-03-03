/* This code is from MicroOS by Glowman554 on GitHub:
	DHCP module: https://github.com/Glowman554/MicroOS/blob/master/user/dhcp/main.c
*/
#include "dhcp.hpp"
#include "netgeneric.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <panic.hpp>
#include <cstdio>
#include "utils.hpp"

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

#define DHCP_REQUEST 1
#define DHCP_REPLY 2

#define FRAME_HEADER_SIZE (sizeof(ethernet_frame) + sizeof(ip_header) + sizeof(udp_header))

static void send_dhcp_packet(dhcp_packet* packet, mac_u src_mac) {
	uint8_t frame[FRAME_HEADER_SIZE + sizeof(dhcp_packet)];
	mem::memset(frame, 0, sizeof(frame));

	ethernet_frame* eth = (ethernet_frame*)frame;
	mem::memset(eth->dst_mac, 0xFF, 6);
	mem::memcpy(eth->src_mac, &src_mac, 6);
	eth->ethertype = __builtin_bswap16(0x0800);

	ip_header* ip = (ip_header*)(frame + sizeof(ethernet_frame));
	size_t ip_size = sizeof(ip_header) + sizeof(udp_header) + sizeof(dhcp_packet);
	ip->version_ihl = 0x45;
	ip->total_length = __builtin_bswap16(ip_size);
	ip->ttl = 64;
	ip->protocol = 17;
	ip->src_ip = 0x00000000;
	ip->dst_ip = 0xFFFFFFFF;
	ip->checksum = ip_checksum(ip, sizeof(ip_header));

	udp_header* udp = (udp_header*)((uint8_t*)ip + sizeof(ip_header));
	size_t udp_size = sizeof(udp_header) + sizeof(dhcp_packet);
	udp->src_port = __builtin_bswap16(68);
	udp->dst_port = __builtin_bswap16(67);
	udp->length = __builtin_bswap16(udp_size);
	udp->checksum = 0;

	mem::memcpy((uint8_t*)udp + sizeof(udp_header), packet, sizeof(dhcp_packet));

	drivers::net::netgeneric::send(frame, sizeof(frame));
}

static dhcp_packet* recv_dhcp_packet(uint8_t* buffer, size_t buf_len, uint32_t xid) {
	while (true) {
		size_t received = drivers::net::netgeneric::listen(buffer, buf_len);
		if (received == 0) return nullptr;
		if (received < FRAME_HEADER_SIZE + sizeof(dhcp_packet)) continue;

		ethernet_frame* eth = (ethernet_frame*)buffer;
		if (__builtin_bswap16(eth->ethertype) != 0x0800) continue;

		ip_header* ip = (ip_header*)(buffer + sizeof(ethernet_frame));
		if ((ip->version_ihl & 0xF0) != 0x40) continue;
		if (ip->protocol != 17) continue;

		size_t ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
		udp_header* udp = (udp_header*)((uint8_t*)ip + ip_hdr_len);
		if (__builtin_bswap16(udp->dst_port) != 68) continue;

		dhcp_packet* dhcp = (dhcp_packet*)((uint8_t*)udp + sizeof(udp_header));
		if (__builtin_bswap32(dhcp->xid) != xid) continue;

		return dhcp;
	}
}

void dhcp_make_packet(dhcp_packet* packet, uint8_t msg_type, uint32_t request_ip, uint32_t server_ip, uint32_t transaction_identifier, char* hostname, mac_u mac) {
	packet->op = DHCP_REQUEST;
	packet->hardware_types = 1;
	packet->hardware_addr_len = 6;
	packet->hops = 0;
	packet->xid = __builtin_bswap32(transaction_identifier);
	mem::memcpy(packet->client_hardware_addr, &mac, 6);

	uint8_t* options = packet->options;
	*((uint32_t*)(options)) = __builtin_bswap32(0x63825363);
	options += 4;

	*(options++) = 53;
	*(options++) = 1;
	*(options++) = msg_type;

	*(options++) = 50;
	*(options++) = 0x04;
	mem::memcpy((uint32_t*)(options), &request_ip, 4);
	options += 4;

	*(options++) = 54;
	*(options++) = 0x04;
	mem::memcpy((uint32_t*)(options), &server_ip, 4);
	options += 4;

	*(options++) = 12;
	*(options++) = strlen(hostname);
	mem::memcpy(options, hostname, strlen(hostname));
	options += strlen(hostname);

	*(options++) = 55;
	*(options++) = 8;
	*(options++) = 0x1;
	*(options++) = 0x3;
	*(options++) = 0x6;
	*(options++) = 0xF;
	*(options++) = 0x2C;
	*(options++) = 0x2E;
	*(options++) = 0x2F;
	*(options++) = 0x39;

	*(options++) = 0xFF;
}

void* dhcp_get_options(dhcp_packet* packet, uint8_t type) {
	uint8_t* options = packet->options + 4;
	uint8_t curr_type = *options;
	while(curr_type != 0xff) {
		uint8_t len = *(options + 1);
		if(curr_type == type) {
			return options + 2;
		}
		options += (2 + len);
		curr_type = *options;
	}
	return NULL;
}

void assert_type(dhcp_packet* packet, uint8_t expected) {
	uint8_t* type = (uint8_t*) dhcp_get_options(packet, 53);
	if (!type) panic("dhcp: missing message type option");
	assert(*type == expected);
}

static uint32_t dhcp_server_ip = 0;

ip_u dhcp_request(uint32_t transaction_identifier, char* hostname, mac_u mac) {
	printf("dhcp: Sending discover\n");

	dhcp_packet packet;
	mem::memset(&packet, 0, sizeof(dhcp_packet));
	dhcp_make_packet(&packet, 1, 0x00000000, 0x00000000, transaction_identifier, hostname, mac);
	send_dhcp_packet(&packet, mac);

	uint8_t buffer[2048];
	dhcp_packet* response = recv_dhcp_packet(buffer, sizeof(buffer), transaction_identifier);
	if (!response) {
		printf("dhcp: No response received!\n");
		return (ip_u){.ip = 0};
	}

	assert_type(response, 2);

	void* sid = dhcp_get_options(response, 54);
	dhcp_server_ip = sid ? *(uint32_t*)sid : 0;

	ip_u ip = {.ip = response->your_ip};
	printf("dhcp: Received offer: %d.%d.%d.%d\n", ip.ip_p[0], ip.ip_p[1], ip.ip_p[2], ip.ip_p[3]);
	return ip;
}

ip_configuration dhcp_request_ip(ip_u offer, uint32_t transaction_identifier, char* hostname, mac_u mac) {
	printf("dhcp: Sending request\n");

	dhcp_packet packet;
	mem::memset(&packet, 0, sizeof(dhcp_packet));
	dhcp_make_packet(&packet, 3, offer.ip, dhcp_server_ip, transaction_identifier, hostname, mac);
	send_dhcp_packet(&packet, mac);

	uint8_t buffer[2048];
	dhcp_packet* response = recv_dhcp_packet(buffer, sizeof(buffer), transaction_identifier);
	if (!response) {
		printf("dhcp: No response received to request!\n");
		return (ip_configuration){};
	}

	assert_type(response, 5);

	ip_configuration ipconfig;
	ipconfig.ip = (ip_u){.ip = response->your_ip};

	void* opt;

	opt = dhcp_get_options(response, 3);
	ipconfig.gateway_ip = (ip_u){.ip = opt ? *(uint32_t*)opt : 0};

	opt = dhcp_get_options(response, 6);
	ipconfig.dns_ip = (ip_u){.ip = opt ? *(uint32_t*)opt : 0};

	opt = dhcp_get_options(response, 1);
	ipconfig.subnet_mask = (ip_u){.ip = opt ? *(uint32_t*)opt : 0};

	printf("dhcp: Received IP: %d.%d.%d.%d\n", ipconfig.ip.ip_p[0], ipconfig.ip.ip_p[1], ipconfig.ip.ip_p[2], ipconfig.ip.ip_p[3]);
	printf("dhcp: Received Gateway: %d.%d.%d.%d\n", ipconfig.gateway_ip.ip_p[0], ipconfig.gateway_ip.ip_p[1], ipconfig.gateway_ip.ip_p[2], ipconfig.gateway_ip.ip_p[3]);
	printf("dhcp: Received DNS: %d.%d.%d.%d\n", ipconfig.dns_ip.ip_p[0], ipconfig.dns_ip.ip_p[1], ipconfig.dns_ip.ip_p[2], ipconfig.dns_ip.ip_p[3]);
	printf("dhcp: Received Subnet: %d.%d.%d.%d\n", ipconfig.subnet_mask.ip_p[0], ipconfig.subnet_mask.ip_p[1], ipconfig.subnet_mask.ip_p[2], ipconfig.subnet_mask.ip_p[3]);

	return ipconfig;
}
