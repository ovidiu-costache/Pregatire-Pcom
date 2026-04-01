#include <arpa/inet.h> /* ntoh, hton and inet_ functions */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "lib.h"
#include "protocols.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

/* Routing table */
struct route_table_entry *rtable;
int rtable_len;

/* Mac table */
struct mac_entry *mac_table;
int mac_table_len;

/*
 Returns a pointer (eg. &rtable[i]) to the best matching route, or NULL if there
 is no matching route.
*/
struct route_table_entry *get_best_route(uint32_t ip_dest) {
    struct route_table_entry *best_route = NULL;
    for (int i = 0; i < rtable_len; i++) {
        if ((ip_dest & rtable[i].mask) == rtable[i].prefix) {
            if (best_route == NULL || ntohl(rtable[i].mask) > ntohl(best_route->mask)) {
                best_route = &rtable[i];
            }
        }
    }
    return best_route;
}

struct mac_entry *get_mac_entry(uint32_t given_ip) {
    for (int i = 0; i < mac_table_len; i++) {
        if (mac_table[i].ip == given_ip) {
            return &mac_table[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
	int interface;
	char packet[MAX_LEN];
	int packet_len;

	/* Don't touch this */
	init();

	/* Code to allocate the MAC and route tables */
	rtable = malloc(sizeof(struct route_table_entry) * 100);
	/* DIE is a macro for sanity checks */
	DIE(rtable == NULL, "memory");

	mac_table = malloc(sizeof(struct  mac_entry) * 100);
	DIE(mac_table == NULL, "memory");
	
	/* Read the static routing table and the MAC table */
	rtable_len = read_rtable("rtable.txt", rtable);
	mac_table_len = read_mac_table(mac_table);

    srand(time(NULL));

	while (1) {
		/* We call get_packet to receive a packet. get_packet returns
		the interface it has received the data from. And writes to
		len the size of the packet. */
		interface = recv_from_all_links(packet, &packet_len);
		DIE(interface < 0, "get_message");
		printf("We have received a packet\n");
		
		/* Extract the Ethernet header from the packet. Since protocols are
		 * stacked, the first header is the ethernet header, the next header is
		 * at m.payload + sizeof(struct ether_header) */
		struct ether_header *eth_hdr = (struct ether_header *) packet;
		struct iphdr *ip_hdr = (struct iphdr *)(packet + sizeof(struct ether_header));
        struct icmp_hdr *icmp_hdr = (struct icmp_hdr *)(packet + sizeof(struct ether_header) + sizeof(struct iphdr));

		/* Check if we got an IPv4 packet */
		if (eth_hdr->ether_type != ntohs(ETHERTYPE_IP)) {
			printf("Ignored non-IPv4 packet\n");
			continue;
		}

        int n = rand() % 2;
        
        if (n == 0) {
            // recalculez checksum si compar
            // caut lpm
            // scad ttl si recalculez checksum
            // schimb mac urile

            // recalculare checksum si verificare
            int old_checksum = ntohs(ip_hdr->check);
            ip_hdr->check = 0;
            int new_cheksum = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

            if (old_checksum != new_cheksum)
                continue;

            ip_hdr->check = new_cheksum;

            // cautare lpm
            struct route_table_entry *best_route = get_best_route(ip_hdr->daddr);

            if (best_route == NULL)
                continue;

            // decrementare ttl si recalculare checksum
            if (ip_hdr->ttl <= 1)
                continue;

            ip_hdr->ttl = ip_hdr->ttl - 1;
            ip_hdr->check = 0;
            ip_hdr->check = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

            // schimbarea adreselor mac
            // mac sursa va fi adresa mea mac
            // mac destinatie va fi adresa mac a lui best_route
            struct mac_entry *next_hop_mac = get_mac_entry(best_route->next_hop);

            if (next_hop_mac == NULL)
                continue;

            memcpy(eth_hdr->ether_dhost, next_hop_mac->mac, 6);
            get_interface_mac(best_route->interface, eth_hdr->ether_shost);

            send_to_link(best_route->interface, packet, packet_len);
        } else {
            // trimit pachetul inapoi cu type 3 si code 0
            memset(icmp_hdr, 0, sizeof(struct icmp_hdr));

            icmp_hdr->mtype = 3;
            icmp_hdr->mcode = 0;

            icmp_hdr->check = htons(ip_checksum((uint16_t *)icmp_hdr, sizeof(struct icmp_hdr)));

            // fac retur la pachet, trebuie inversate ip urile
            int old_saddr = ip_hdr->saddr;
            ip_hdr->daddr = old_saddr;

            // ip ul sursa devine ip ul routerului curent
            inet_pton(AF_INET, get_interface_ip(interface), &ip_hdr->saddr);

            ip_hdr->protocol = 1; // icmp
            ip_hdr->ttl = 64;

            ip_hdr->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmp_hdr));

            ip_hdr->check = 0;
            ip_hdr->check = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

            memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
            get_interface_mac(interface, eth_hdr->ether_shost);

            int new_len = sizeof(struct ether_header) + ntohs(ip_hdr->tot_len);
            send_to_link(interface, packet, new_len);
        }
	}
}
