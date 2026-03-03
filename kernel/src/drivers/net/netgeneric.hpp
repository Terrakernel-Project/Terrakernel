#ifndef NETGENERIC_HPP
#define NETGENERIC_HPP 1

#include <cstdint>
#include <cstddef>
#include <pcie/pcie.hpp>
#include "unions.hpp"
#include "dns/dns.hpp"
#include "udp/udp.hpp"
#include "arp/arp.hpp"
#include "icmp/icmp.hpp"
#include "tcpip/tcpip.hpp"

struct net_stats {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t errors;
};

struct net_card_driver {
    const char* name;
    bool (*init)(pcie_device* dev, net_card_driver* driver);
    bool (*send)(const uint8_t* data, size_t length);
    size_t (*receive)(uint8_t* buffer, size_t len);
    size_t (*listen)(uint8_t* buffer, size_t len);
    bool (*get_mac)(uint8_t mac[6]);
};

namespace drivers::net::netgeneric {

void initialise();

bool send(const uint8_t* data, size_t length);
size_t recv(uint8_t* buffer, size_t buffer_len);
size_t listen(uint8_t* buffer, size_t buffer_len);

bool get_mac(uint8_t mac[6]);
ip_u get_ip();

net_stats net_card_get_stats();

}

#endif
