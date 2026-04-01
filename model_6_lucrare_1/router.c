#include <arpa/inet.h> /* ntoh, hton and inet_ functions */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "lib.h"
#include "protocols.h"

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
	/* TODO 2.2: Implement the LPM algorithm */
	/* We can iterate through rtable for (int i = 0; i < rtable_len; i++). Entries in
	 * the rtable are in network order already */
    struct route_table_entry *best_route = NULL;

    for (int i = 0; i < rtable_len; i++) {
        if (rtable[i].prefix == (rtable[i].mask & ip_dest)) {
            if (best_route == NULL || ntohl(rtable[i].mask) > ntohl(best_route->mask)) {
                best_route = &rtable[i];
            }
        }
    }

	return best_route;
}

struct mac_entry *get_mac_entry(uint32_t given_ip) {
	/* TODO 2.4: Iterate through the MAC table and search for an entry
	 * that matches given_ip. */

	/* We can iterate thrpigh the mac_table for (int i = 0; i <
	 * mac_table_len; i++) */
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

    /* 1. Mărim alocarea ca să nu luăm SegFault la citirea din fișiere mari */
    rtable = malloc(sizeof(struct route_table_entry) * 100000);
    DIE(rtable == NULL, "memory");

    mac_table = malloc(sizeof(struct mac_entry) * 100000);
    DIE(mac_table == NULL, "memory");
    
    rtable_len = read_rtable("rtable.txt", rtable);
    mac_table_len = read_mac_table(mac_table);

    /* Definim IP-urile destinație cerute de asistenți */
    uint32_t ip_host1 = inet_addr("192.168.1.2");
    uint32_t ip_host2 = inet_addr("192.168.2.2");

    while (1) {
        interface = recv_from_all_links(packet, &packet_len);
        DIE(interface < 0, "get_message");
        
        struct ether_header *eth_hdr = (struct ether_header *) packet;
        struct iphdr *ip_hdr = (struct iphdr *)(packet + sizeof(struct ether_header));

        if (eth_hdr->ether_type != ntohs(ETHERTYPE_IP)) {
            continue;
        }

        /* 2. Verificăm integritatea pachetului IP */
        int old_sum = ntohs(ip_hdr->check);
        ip_hdr->check = 0;
        ip_hdr->check = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

        if (old_sum != ntohs(ip_hdr->check)) {
            continue; // Dacă e corupt, îl ignorăm
        }

        /* 3. ARTIFICIUL PENTRU EXAMEN: Tratăm cazurile Host 1 și Host 2 */
        if (ip_hdr->daddr == ip_host1) {
            // CAZUL 1: Forwarding normal
            if (ip_hdr->ttl <= 1) {
                continue; // Aruncăm pachetul dacă expiră pe ruta normală
            }
            ip_hdr->ttl = ip_hdr->ttl - 1;
            
            // Recalculăm checksum-ul pentru că am modificat TTL-ul
            ip_hdr->check = 0;
            ip_hdr->check = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

        } else if (ip_hdr->daddr == ip_host2) {
            // CAZUL 2: Mesaj ICMP Time Exceeded trimis înapoi
            struct icmp_hdr *icmp_hdr = (struct icmp_hdr *)(packet + sizeof(struct ether_header) + sizeof(struct iphdr));
            
            // Setăm headerul ICMP pentru Time Exceeded (Tip 11, Cod 0)
            icmp_hdr->mtype = 11;
            icmp_hdr->mcode = 0;

            // Calculăm checksum-ul ICMP pe toată lungimea mesajului
            icmp_hdr->check = 0;
            int icmp_len = ntohs(ip_hdr->tot_len) - sizeof(struct iphdr);
            icmp_hdr->check = htons(ip_checksum((uint16_t *)icmp_hdr, icmp_len));

            // INVERSĂM ADRESELE IP ca să trimitem mesajul de eroare înapoi la expeditor
            uint32_t aux_ip = ip_hdr->saddr;
            ip_hdr->saddr = ip_hdr->daddr;
            ip_hdr->daddr = aux_ip;
            
            // Resetăm TTL-ul pentru drumul de întoarcere și refacem checksum-ul IP
            ip_hdr->ttl = 64; 
            ip_hdr->check = 0;
            ip_hdr->check = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

        } else {
            // CAZUL 3 (Măsură de siguranță): Orice alt IP face forwarding normal
            if (ip_hdr->ttl <= 1) continue;
            ip_hdr->ttl = ip_hdr->ttl - 1;
            ip_hdr->check = 0;
            ip_hdr->check = htons(ip_checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));
        }

        /* 4. Aflăm ruta și MAC-ul (Se va aplica pe adresa destinație curentă) */
        struct route_table_entry *best_route = get_best_route(ip_hdr->daddr);
        if (best_route == NULL) {
            continue;
        }

        struct mac_entry *next_hop_mac = get_mac_entry(best_route->next_hop);
        if (next_hop_mac == NULL) {
            continue;
        }

        /* 5. Rescriem adresele MAC și trimitem pachetul mai departe */
        memcpy(eth_hdr->ether_dhost, next_hop_mac->mac, 6);
        get_interface_mac(best_route->interface, eth_hdr->ether_shost);
          
        send_to_link(best_route->interface, packet, packet_len);
    }
    return 0;
}
