#include "../udp/udp.hpp"
#include "../netgeneric.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <cstdio>

struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

#define DNS_FLAG_RD     0x0100
#define DNS_FLAG_QR     0x8000
#define DNS_FLAG_AA     0x0400
#define DNS_FLAG_TC     0x0200
#define DNS_FLAG_RA     0x0080
#define DNS_TYPE_A      0x0001
#define DNS_CLASS_IN    0x0001

#define DNS_PORT        53
#define DNS_MAX_PACKET  512

static ip_u dns_server_ip = {.ip = 0};
static uint16_t dns_id = 0x1234;

void dns_set_server(ip_u server_ip) {
    dns_server_ip = server_ip;
}

static uint8_t* encode_name(uint8_t* ptr, const char* name) {
    while (*name) {
        const char* dot = name;
        while (*dot && *dot != '.') dot++;
        uint8_t len = dot - name;
        *ptr++ = len;
        mem::memcpy(ptr, name, len);
        ptr += len;
        if (*dot == '.') dot++;
        name = dot;
    }
    *ptr++ = 0;
    return ptr;
}

static uint8_t* skip_name(uint8_t* ptr, uint8_t* start) {
    while (*ptr) {
        if ((*ptr & 0xC0) == 0xC0) {
            ptr += 2;
            return ptr;
        }
        ptr += *ptr + 1;
    }
    return ptr + 1;
}

namespace drivers::net::dns {

ip_u dns_lookup(const char* hostname) {
    if (dns_server_ip.ip == 0) {
        printf("dns: no server configured\n\r");
        return {.ip = 0};
    }

    uint8_t packet[DNS_MAX_PACKET];
    mem::memset(packet, 0, sizeof(packet));

    dns_header* hdr = (dns_header*)packet;
    hdr->id      = __builtin_bswap16(dns_id++);
    hdr->flags   = __builtin_bswap16(DNS_FLAG_RD);
    hdr->qdcount = __builtin_bswap16(1);

    uint8_t* ptr = packet + sizeof(dns_header);
    ptr = encode_name(ptr, hostname);
    *(uint16_t*)ptr = __builtin_bswap16(DNS_TYPE_A);  ptr += 2;
    *(uint16_t*)ptr = __builtin_bswap16(DNS_CLASS_IN); ptr += 2;

    size_t send_len = ptr - packet;

    if (!drivers::net::udp::udp_send_packet(packet, send_len, dns_server_ip)) {
        printf("dns: failed to send query\n\r");
        return {.ip = 0};
    }

    uint8_t buffer[DNS_MAX_PACKET];
    if (!drivers::net::udp::udp_listen_packet(buffer, sizeof(buffer), dns_server_ip)) {
        printf("dns: no response\n\r");
        return {.ip = 0};
    }

    dns_header* resp = (dns_header*)buffer;
    if ((__builtin_bswap16(resp->flags) & DNS_FLAG_QR) == 0) {
        printf("dns: response is not a reply\n\r");
        return {.ip = 0};
    }

    uint16_t ancount = __builtin_bswap16(resp->ancount);
    if (ancount == 0) {
        printf("dns: no answers for %s\n\r", hostname);
        return {.ip = 0};
    }

    uint8_t* rptr = buffer + sizeof(dns_header);
    for (uint16_t i = 0; i < __builtin_bswap16(resp->qdcount); i++) {
        rptr = skip_name(rptr, buffer);
        rptr += 4;
    }

    for (uint16_t i = 0; i < ancount; i++) {
        rptr = skip_name(rptr, buffer);

        uint16_t type  = __builtin_bswap16(*(uint16_t*)rptr); rptr += 2;
        uint16_t cls   = __builtin_bswap16(*(uint16_t*)rptr); rptr += 2;
        rptr += 4;
        uint16_t rdlen = __builtin_bswap16(*(uint16_t*)rptr); rptr += 2;

        if (type == DNS_TYPE_A && cls == DNS_CLASS_IN && rdlen == 4) {
            ip_u result;
            mem::memcpy(&result.ip, rptr, 4);
            printf("dns: %s -> %d.%d.%d.%d\n\r", hostname,
                result.ip_p[0], result.ip_p[1],
                result.ip_p[2], result.ip_p[3]);
            return result;
        }

        rptr += rdlen;
    }

    printf("dns: no A record found for %s\n\r", hostname);
    return {.ip = 0};
}

}
