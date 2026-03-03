#include "../dhcp.hpp"
#include "../netgeneric.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <cstdio>
#include <panic.hpp>

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

struct tcp_header {
	uint16_t src_port;
	uint16_t dst_port;
	uint32_t seq;
	uint32_t ack;
	uint8_t  data_offset;
	uint8_t  flags;
	uint16_t window;
	uint16_t checksum;
	uint16_t urgent;
} __attribute__((packed));

#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

enum tcp_state {
	TCP_CLOSED,
	TCP_SYN_SENT,
	TCP_ESTABLISHED,
	TCP_FIN_WAIT,
};

static ip_u tcp_src_ip    = {.ip = 0};
static uint16_t tcp_src_port = 0;
static uint16_t tcp_dst_port = 0;
static uint32_t tcp_seq      = 0;
static uint32_t tcp_ack      = 0;
static tcp_state tcp_conn_state = TCP_CLOSED;
static ip_u tcp_remote_ip = {.ip = 0};

void tcpip_set_config(ip_u src_ip, uint16_t src_port, uint16_t dst_port) {
	tcp_src_ip   = src_ip;
	tcp_src_port = src_port;
	tcp_dst_port = dst_port;
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

struct tcp_pseudo_header {
	uint32_t src_ip;
	uint32_t dst_ip;
	uint8_t  zero;
	uint8_t  protocol;
	uint16_t tcp_length;
} __attribute__((packed));

static uint16_t tcp_checksum(ip_u src, ip_u dst, tcp_header* tcp, size_t tcp_len) {
	size_t pseudo_len = sizeof(tcp_pseudo_header) + tcp_len;
	uint8_t* buf = (uint8_t*)mem::heap::malloc(pseudo_len);
	if (!buf) return 0;
	mem::memset(buf, 0, pseudo_len);

	tcp_pseudo_header* ph = (tcp_pseudo_header*)buf;
	ph->src_ip     = src.ip;
	ph->dst_ip     = dst.ip;
	ph->zero       = 0;
	ph->protocol   = 6;
	ph->tcp_length = __builtin_bswap16(tcp_len);
	mem::memcpy(buf + sizeof(tcp_pseudo_header), tcp, tcp_len);

	uint16_t result = ip_checksum(buf, pseudo_len);
	mem::heap::free(buf);
	return result;
}

static bool send_tcp_segment(ip_u dst_ip, uint8_t flags, const uint8_t* data, size_t data_len) {
	size_t tcp_len   = sizeof(tcp_header) + data_len;
	size_t total     = sizeof(ethernet_frame) + sizeof(ip_header) + tcp_len;
	uint8_t* frame   = (uint8_t*)mem::heap::malloc(total);
	if (!frame) return false;
	mem::memset(frame, 0, total);

	uint8_t src_mac[6];
	drivers::net::netgeneric::get_mac(src_mac);

	ethernet_frame* eth = (ethernet_frame*)frame;
	mem::memset(eth->dst_mac, 0xFF, 6);
	mem::memcpy(eth->src_mac, src_mac, 6);
	eth->ethertype = __builtin_bswap16(0x0800);

	ip_header* ip = (ip_header*)(frame + sizeof(ethernet_frame));
	size_t ip_size = sizeof(ip_header) + tcp_len;
	ip->version_ihl   = 0x45;
	ip->total_length  = __builtin_bswap16(ip_size);
	ip->identification = __builtin_bswap16(0x1337);
	ip->ttl           = 64;
	ip->protocol      = 6;
	ip->src_ip        = tcp_src_ip.ip;
	ip->dst_ip        = dst_ip.ip;
	ip->checksum      = ip_checksum(ip, sizeof(ip_header));

	tcp_header* tcp = (tcp_header*)((uint8_t*)ip + sizeof(ip_header));
	tcp->src_port   = __builtin_bswap16(tcp_src_port);
	tcp->dst_port   = __builtin_bswap16(tcp_dst_port);
	tcp->seq        = __builtin_bswap32(tcp_seq);
	tcp->ack        = __builtin_bswap32(tcp_ack);
	tcp->data_offset = (sizeof(tcp_header) / 4) << 4;
	tcp->flags      = flags;
	tcp->window     = __builtin_bswap16(65535);
	tcp->checksum   = 0;

	if (data && data_len > 0) {
		mem::memcpy((uint8_t*)tcp + sizeof(tcp_header), data, data_len);
	}

	tcp->checksum = tcp_checksum(tcp_src_ip, dst_ip, tcp, tcp_len);

	bool ok = drivers::net::netgeneric::send(frame, total);
	mem::heap::free(frame);
	return ok;
}

static bool recv_tcp_segment(uint8_t* buffer, size_t length, ip_u target_ip, uint8_t expected_flags) {
	while (true) {
		size_t received = drivers::net::netgeneric::listen(buffer, length);
		if (received < sizeof(ethernet_frame) + sizeof(ip_header) + sizeof(tcp_header)) continue;

		ethernet_frame* eth = (ethernet_frame*)buffer;
		if (__builtin_bswap16(eth->ethertype) != 0x0800) continue;

		ip_header* ip = (ip_header*)(buffer + sizeof(ethernet_frame));
		if ((ip->version_ihl & 0xF0) != 0x40) continue;
		if (ip->protocol != 6) continue;
		if (target_ip.ip != 0 && ip->src_ip != target_ip.ip) continue;

		size_t ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
		tcp_header* tcp = (tcp_header*)((uint8_t*)ip + ip_hdr_len);
		if (__builtin_bswap16(tcp->dst_port) != tcp_src_port) continue;
		if (tcp->flags & TCP_RST) {
			tcp_conn_state = TCP_CLOSED;
			return false;
		}
		if ((tcp->flags & expected_flags) != expected_flags) continue;

		tcp_ack = __builtin_bswap32(tcp->seq) + 1;
		return true;
	}
}

namespace drivers::net::tcpip {

bool tcpip_send_packet(const uint8_t* data, size_t length, ip_u target_ip) {
	uint8_t buffer[2048];

	if (tcp_conn_state == TCP_CLOSED || tcp_remote_ip.ip != target_ip.ip) {
		printf("tcp: Connecting to %d.%d.%d.%d\n\r",
			target_ip.ip_p[0], target_ip.ip_p[1],
			target_ip.ip_p[2], target_ip.ip_p[3]);

		tcp_seq = 0xDEADBEEF;
		tcp_ack = 0;
		tcp_remote_ip = target_ip;

		if (!send_tcp_segment(target_ip, TCP_SYN, nullptr, 0)) return false;
		tcp_conn_state = TCP_SYN_SENT;
		tcp_seq++;

		if (!recv_tcp_segment(buffer, sizeof(buffer), target_ip, TCP_SYN | TCP_ACK)) return false;

		if (!send_tcp_segment(target_ip, TCP_ACK, nullptr, 0)) return false;
		tcp_conn_state = TCP_ESTABLISHED;
		printf("tcp: Connection established\n\r");
	}

	if (!send_tcp_segment(target_ip, TCP_PSH | TCP_ACK, data, length)) return false;
	tcp_seq += length;

	if (!recv_tcp_segment(buffer, sizeof(buffer), target_ip, TCP_ACK)) return false;

	return true;
}

bool tcpip_recv_packet(uint8_t* buffer, size_t length, ip_u target_ip) {
	if (length < sizeof(ethernet_frame) + sizeof(ip_header) + sizeof(tcp_header)) return false;

	ethernet_frame* eth = (ethernet_frame*)buffer;
	if (__builtin_bswap16(eth->ethertype) != 0x0800) return false;

	ip_header* ip = (ip_header*)(buffer + sizeof(ethernet_frame));
	if ((ip->version_ihl & 0xF0) != 0x40) return false;
	if (ip->protocol != 6) return false;
	if (target_ip.ip != 0 && ip->src_ip != target_ip.ip) return false;

	size_t ip_hdr_len = (ip->version_ihl & 0x0F) * 4;
	tcp_header* tcp = (tcp_header*)((uint8_t*)ip + ip_hdr_len);
	if (__builtin_bswap16(tcp->dst_port) != tcp_src_port) return false;
	if (tcp->flags & TCP_RST) {
		tcp_conn_state = TCP_CLOSED;
		return false;
	}

	size_t tcp_hdr_len = (tcp->data_offset >> 4) * 4;
	size_t payload_len = __builtin_bswap16(ip->total_length) - sizeof(ip_header) - tcp_hdr_len;

	if (payload_len == 0) return false;

	uint8_t* payload = (uint8_t*)tcp + tcp_hdr_len;
	mem::memmove(buffer, payload, payload_len);

	tcp_ack = __builtin_bswap32(tcp->seq) + payload_len;
	send_tcp_segment(target_ip, TCP_ACK, nullptr, 0);

	return true;
}

bool tcpip_listen_packet(uint8_t* buffer, size_t length, ip_u target_ip) {
	while (true) {
		size_t received = drivers::net::netgeneric::listen(buffer, length);
		if (received == 0) continue;
		if (tcpip_recv_packet(buffer, received, target_ip)) return true;
	}
}

void tcpip_close(ip_u target_ip) {
	if (tcp_conn_state != TCP_ESTABLISHED) return;
	uint8_t buffer[512];
	tcp_conn_state = TCP_FIN_WAIT;
	send_tcp_segment(target_ip, TCP_FIN | TCP_ACK, nullptr, 0);
	tcp_seq++;
	recv_tcp_segment(buffer, sizeof(buffer), target_ip, TCP_ACK);
	tcp_conn_state = TCP_CLOSED;
	printf("tcp: Connection closed\n\r");
}

}
