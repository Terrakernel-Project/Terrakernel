#ifndef DNS_HPP
#define DNS_HPP 1

#include <drivers/net/utils.hpp>

void dns_set_server(ip_u server_ip);

namespace drivers::net::dns {

ip_u dns_lookup(const char* hostname);

}

#endif
