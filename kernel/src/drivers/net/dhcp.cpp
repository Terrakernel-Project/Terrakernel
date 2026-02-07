/* This code is from MicroOS by Glowman554 on GitHub:
	DHCP module: https://github.com/Glowman554/MicroOS/blob/master/user/dhcp/main.c
*/

#include "dhcp.hpp"
#include "netgeneric.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <panic.hpp>

char* __libc_parse_number(char* input, int* output) {
	int idx = 0;
	int number_system_base = 10;

	if (input[0] == '0') {
		if (input[1] == 'x') {
			number_system_base = 16;
			idx = 2;
		} else if (input[1] == 'b') {
			number_system_base = 2;
			idx = 2;
		}
	}

	int _number = 0;

	while (input[idx] != '\0') {
		if (input[idx] >= '0' && input[idx] <= '9') {
			_number = _number * number_system_base + (input[idx] - '0');
		} else if (input[idx] >= 'a' && input[idx] <= 'f') {
			_number = _number * number_system_base + (input[idx] - 'a' + 10);
		} else if (input[idx] >= 'A' && input[idx] <= 'F') {
			_number = _number * number_system_base + (input[idx] - 'A' + 10);
		} else {
			break;
		}

		idx++;
	}

	*output = _number;

	return &input[idx];
}

ip_u parse_ip(const char* in) {
	ip_u ip = { 0 };

	char* curr = (char*) in;
	for (int i = 0; i < 4; i++) {
		int r = 0;
		curr = __libc_parse_number(curr, &r);
		if ((*curr != '.' && *curr != 0) || (*curr == 0 && i != 3)) {
			return (ip_u) {.ip = 0};
		}
		curr++;
		ip.ip_p[i] = r;
	}

	return ip;
}

#define DHCP_REQUEST 1
#define DHCP_REPLY 2

void dhcp_make_packet(dhcp_packet* packet, uint8_t msg_type, uint32_t request_ip, uint32_t transaction_identifier, char* hostname, mac_u mac) {
	packet->op = DHCP_REQUEST;
	packet->hardware_types = 1;
	packet->hardware_addr_len = 6;
	packet->hops = 0;
	packet->xid = __builtin_bswap32(transaction_identifier);
	mem::memcpy(packet->client_hardware_addr, &mac, 6);

	uint8_t dst_ip[4];
	mem::memset(dst_ip, 0xFF, 4);

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

	*(options++) = 12;
	*(options++) = 1 + strlen(hostname);
	mem::memcpy(options, hostname, strlen(hostname));
	options += strlen(hostname);
	*(options++) = 0x00;

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
    assert(*type == expected);
}

ip_u dhcp_request(uint32_t transaction_identifier, char* hostname, mac_u mac) {
    printf("dhcp: Sending discover\n");

	printf("step 1\n\r");
    dhcp_packet packet;
	printf("step 2\n\r");
    mem::memset(&packet, 0, sizeof(dhcp_packet));
	printf("step 3\n\r");

    dhcp_make_packet(&packet, 1, 0x00000000, transaction_identifier, hostname, mac);
	printf("step 4\n\r");
    drivers::net::netgeneric::send((uint8_t*)&packet, sizeof(dhcp_packet));
	printf("step 5\n\r");

    uint8_t buffer[2048];
	printf("step 6\n\r");
    
    size_t received = drivers::net::netgeneric::recv(buffer, sizeof(buffer));
	printf("step 7 (received %zu bytes)\n\r", received);

    if (received == 0) {
        printf("dhcp: No response received!\n");
        return (ip_u){.ip = 0};
    }

    dhcp_packet* response = (dhcp_packet*) buffer;
	printf("step 8\n\r");
    assert_type(response, 2);
	printf("step 9\n\r");

    ip_u ip = {.ip = response->your_ip};
	printf("step 10\n\r");
    printf("dhcp: Received offer: %d.%d.%d.%d\n", ip.ip_p[0], ip.ip_p[1], ip.ip_p[2], ip.ip_p[3]);
	printf("step 11\n\r");

    return ip;
}

ip_configuration dhcp_request_ip(ip_u offer, uint32_t transaction_identifier, char* hostname, mac_u mac) {
    printf("dhcp: Sending request\n");

	dhcp_packet packet;
	mem::memset(&packet, 0, sizeof(dhcp_packet));

	dhcp_make_packet(&packet, 3, offer.ip, transaction_identifier, hostname, mac);
    drivers::net::netgeneric::send((uint8_t*)&packet, sizeof(dhcp_packet));

    uint8_t buffer[2048];
    size_t received = drivers::net::netgeneric::recv(buffer, sizeof(buffer));

    if (received == 0) {
        printf("dhcp: No response received to request!\n");
        return (ip_configuration){};
    }

    dhcp_packet* response = (dhcp_packet*) buffer;
    assert_type(response, 5);

	ip_configuration ipconfig;
	ipconfig.ip = (ip_u) {.ip = response->your_ip};
	ipconfig.gateway_ip = (ip_u) {.ip = *(uint32_t*) dhcp_get_options(response, 3)};
	ipconfig.dns_ip = (ip_u) {.ip = *(uint32_t*) dhcp_get_options(response, 6)};
	ipconfig.subnet_mask = (ip_u) {.ip = *(uint32_t*) dhcp_get_options(response, 1)};

    printf("dhcp: Received IP: %d.%d.%d.%d\n", ipconfig.ip.ip_p[0], ipconfig.ip.ip_p[1], ipconfig.ip.ip_p[2], ipconfig.ip.ip_p[3]);
	printf("dhcp: Received Gateway: %d.%d.%d.%d\n", ipconfig.gateway_ip.ip_p[0], ipconfig.gateway_ip.ip_p[1], ipconfig.gateway_ip.ip_p[2], ipconfig.gateway_ip.ip_p[3]);
	printf("dhcp: Received DNS: %d.%d.%d.%d\n", ipconfig.dns_ip.ip_p[0], ipconfig.dns_ip.ip_p[1], ipconfig.dns_ip.ip_p[2], ipconfig.dns_ip.ip_p[3]);
	printf("dhcp: Received Subnet: %d.%d.%d.%d\n", ipconfig.subnet_mask.ip_p[0], ipconfig.subnet_mask.ip_p[1], ipconfig.subnet_mask.ip_p[2], ipconfig.subnet_mask.ip_p[3]);

	return ipconfig;
}
