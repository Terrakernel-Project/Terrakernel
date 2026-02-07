#ifndef NETGENERIC_HPP
#define NETGENERIC_HPP 1

#include <cstdint>
#include <cstddef>

struct net_stats {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t errors;
};

struct net_card_driver {
    const char* name;
    bool (*init)();
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

net_stats net_card_get_stats();

void install_net_card(net_card_driver* driver);

}

#endif
