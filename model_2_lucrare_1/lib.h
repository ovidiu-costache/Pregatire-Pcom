#include <stdint.h>

/* 
 *Note that "buffer" should be at least the MTU size of the 
 * interface, eg 1500 bytes 
 */
#define MAX_LEN 1500
#define ROUTER_NUM_INTERFACES 4

/* MAC Table Entry */
struct mac_entry {
	int32_t ip;
	uint8_t mac[6];
};

/* Route Table Entry */
struct route_table_entry {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
};

/* Sends a packet on an interface. */
int send_to_link(int interface, char *packet, int len);

/* Receives a packet. Returns the interface it has been received from. */
int recv_from_all_links(char *packet, int *len);

/* Utility functions below */

/* Returns the checksum of an IP header */
uint16_t ip_checksum(uint16_t *vdata, size_t len);

/* Returns the ip address of a ninterface */
char *get_interface_ip(int interface);

/* Write to mac, a uint8_t mac[6] the MAC address of an interface */
int get_interface_mac(int interface, uint8_t mac[6]);






/* Extra function */
size_t read_mac_table(struct mac_entry *mac_table);

int read_rtable(const char *path, struct route_table_entry *rtable);

/* Macro for sanity checks */
#define DIE(condition, message) \
	do { \
		if ((condition)) { \
			fprintf(stderr, "[%d]: %s\n", __LINE__, (message)); \
			perror(""); \
			exit(1); \
		} \
	} while (0)

void init();
