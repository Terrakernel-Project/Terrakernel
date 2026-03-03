#ifndef TCPIP_HPP
#define TCPIP_HPP 1

#include <cstdint>
#include <cstddef>
#include <drivers/net/utils.hpp>

void tcpip_set_config(ip_u src_ip, uint16_t src_port, uint16_t dst_port);

namespace drivers::net::tcpip {

bool tcpip_send_packet(const uint8_t* data, size_t length, ip_u target_ip);
bool tcpip_recv_packet(uint8_t* buffer, size_t length, ip_u target_ip);
bool tcpip_listen_packet(uint8_t* buffer, size_t length, ip_u target_ip);
void tcpip_close(ip_u target_ip);

}

#endif
