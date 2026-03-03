#ifndef UDP_HPP
#define UDP_HPP 1

#include <cstdint>
#include <cstddef>
#include <drivers/net/utils.hpp>

void udp_set_config(ip_u src_ip, uint16_t src_port, uint16_t dst_port);
void udp_set_gateway(ip_u gateway_ip);

namespace drivers::net::udp {

bool udp_send_packet(const uint8_t* data, size_t length, ip_u target_ip);
bool udp_recv_packet(uint8_t* buffer, size_t length, ip_u target_ip);
bool udp_listen_packet(uint8_t* buffer, size_t length, ip_u target_ip);

}

#endif
