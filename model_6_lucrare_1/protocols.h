#include <unistd.h>
#include <stdint.h>

#ifndef ETHERTYPE_IP
#define ETHERTYPE_IP		0x0800	/* IP protocol */
#endif

struct  ether_header {
    uint8_t  ether_dhost[6];
    uint8_t  ether_shost[6];
    uint16_t ether_type;     // ETHERTYPE_IP
};

struct iphdr {
    uint8_t   ihl:4, version:4;// don't care
    uint8_t    tos;      // don't care
    uint16_t   tot_len;  // don't care
    uint16_t   id;       // don't care
    uint16_t   frag_off; // don't care
    uint8_t    ttl;      // Time to Live -> to avoid loops, we will decrement
    uint8_t    protocol; // don't care
    uint16_t   check;    // checksum     -> Since we modify TTL,
    // we need to recompute the checksum
    uint32_t   saddr;    // don't care
    uint32_t   daddr;    // the destination of the packet
};
