#ifndef DHCP_HPP
#define DHCP_HPP 1

#include <cstdint>

struct dhcp_packet {
	uint8_t op;
	uint8_t hardware_types;
	uint8_t hardware_addr_len;
	uint8_t hops;
	uint32_t xid;
	uint16_t seconds;
	uint16_t flags;
	uint32_t client_ip;
	uint32_t your_ip;
	uint32_t server_ip;
	uint32_t gateway_ip;
	uint8_t client_hardware_addr[16];
	uint8_t server_name[64];
	uint8_t file[128];
	uint8_t options[64];
} __attribute__((packed));

typedef union ip {
	uint8_t ip_p[4];
	uint32_t ip;
} ip_u;

typedef union mac {
	uint8_t mac_p[6];
	uint64_t mac;
} mac_u;

typedef struct ip_configuration {
	ip_u ip;
	ip_u subnet_mask;
	ip_u gateway_ip;
	ip_u dns_ip;
} ip_configuration_t;

ip_u parse_ip(const char* in);

// form a DHCP packet
void dhcp_make_packet(dhcp_packet* packet, uint8_t msg_type, uint32_t request_ip, uint32_t transaction_identifier, char* hostname, mac_u mac);

/**
 * @brief Sends a DHCP Discover packet and receives a DHCP Offer
 *
 * This function constructs a DHCP Discover packet using the provided
 * transaction identifier, hostname, and MAC address, sends it over the
 * network, waits for a DHCP Offer response, and returns the offered IP address.
 */
ip_u dhcp_request(uint32_t transaction_identifier, char* hostname, mac_u mac);

/**
 * @brief Sends a DHCP Request for a given offered IP and receives the final configuration
 *
 * This function sends a DHCP Request packet to the DHCP server, asking to
 * lease the provided offered IP. It waits for the DHCP ACK response and
 * extracts the network configuration including:
 *   - IP address
 *   - Gateway
 *   - DNS server
 *   - Subnet mask
 *
 * @param offer The IP address offered by the DHCP server
 * @param transaction_identifier Unique transaction ID for the DHCP session
 * @param hostname Hostname to include in the DHCP request
 * @param mac MAC address of the network interface
 * @return ip_configuration Struct containing the IP configuration assigned by the DHCP server
 */
ip_configuration dhcp_request_ip(ip_u offer, uint32_t transaction_identifier, char* hostname, mac_u mac);

#endif
