#ifndef ICMP_HPP
#define ICMP_HPP 1

#include <drivers/net/unions.hpp>

void icmp_set_config(ip_u src_ip, ip_u gateway_ip);

namespace drivers::net::icmp {

bool icmp_ping(ip_u target_ip, uint16_t sequence, uint32_t* rtt_ms);
void icmp_ping_print(ip_u target_ip, int count);

}

#endif
