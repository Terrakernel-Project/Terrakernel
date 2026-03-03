#ifndef ARP_HPP
#define ARP_HPP 1

#include <cstdint>
#include <cstddef>
#include <drivers/net/unions.hpp>

void arp_set_src_ip(ip_u src_ip);

namespace drivers::net::arp {

bool arp_lookup(ip_u target_ip, uint8_t mac_out[6]);
void arp_handle_packet(uint8_t* frame, size_t length);

}

#endif
